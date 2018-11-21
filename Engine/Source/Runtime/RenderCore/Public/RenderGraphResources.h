// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "RendererInterface.h"


class FRDGResource;
class FRDGTexture;
class FRDGTextureSRV;
class FRDGTextureUAV;

class FRDGBuffer;
class FRDGBufferSRV;
class FRDGBufferUAV;

struct FPooledRDGBuffer;


/** Generic graph resource. */
class RENDERCORE_API FRDGResource
{
public:
	// Name of the resource for debugging purpose.
	const TCHAR* const Name = nullptr;

	FRDGResource(const TCHAR* InDebugName)
		: Name(InDebugName)
	{
		CachedRHI.Resource = nullptr;
	}

	FRDGResource() = delete;
	FRDGResource(const FRDGResource&) = delete;
	void operator = (const FRDGResource&) = delete;

private:
	// RHI resource once allocated.
	union
	{
		mutable FRHIResource* Resource;
		mutable FShaderResourceViewRHIParamRef SRV;
		mutable FUnorderedAccessViewRHIParamRef UAV;
	} CachedRHI;

	/** Number of references in passes and deferred queries. */
	mutable int32 ReferenceCount = 0;

	// Used for tracking resource state during execution
	mutable bool bWritable = false;
	mutable bool bCompute = false;

	/** Boolean to tracked whether a ressource is actually used by the lambda of a pass or not. */
	mutable bool bIsActuallyUsedByPass = false;

	friend class FRDGBuilder;


	/** Friendship over parameter settings for bIsActuallyUsedByPass. It means only this parameter settings can be
	 * used for render graph resource, to force the useless dependency tracking to just work.
	 */
	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);

	template<typename TRHICmdList, typename TShaderClass>
	friend void SetShaderUAVs(TRHICmdList&, const TShaderClass*, FComputeShaderRHIParamRef, const typename TShaderClass::FParameters&);
};

/** Descriptor of a graph tracked texture. */
using FRDGTextureDesc = FPooledRenderTargetDesc;

/** Render graph tracked Texture. */
class RENDERCORE_API FRDGTexture : public FRDGResource
{
public:
	//TODO(RDG): using FDesc = FPooledRenderTargetDesc;

	/** Descriptor of the graph tracked texture. */
	const FPooledRenderTargetDesc Desc;

private:
	/** This is not a TRefCountPtr<> because FRDGTexture is allocated on the FMemStack
	 * FGraphBuilder::AllocatedTextures is actually keeping the reference.
	 */
	mutable IPooledRenderTarget* PooledRenderTarget = nullptr;

	FRDGTexture(const TCHAR* DebugName, const FPooledRenderTargetDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	/** Returns the allocated RHI texture. */
	inline FTextureRHIParamRef GetRHITexture() const
	{
		check(PooledRenderTarget);
		return PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
	}

	friend class FRDGBuilder;

	template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
	friend void SetShaderParameters(TRHICmdList&, const TShaderClass*, TShaderRHI*, const typename TShaderClass::FParameters&);
};

/** Decsriptor for render graph tracked SRV. */
class RENDERCORE_API FRDGTextureSRVDesc
{
public:
	const FRDGTexture* Texture;
	uint8 MipLevel = 0;

	FRDGTextureSRVDesc() {}

	FRDGTextureSRVDesc(const FRDGTexture* InTexture, uint8 InMipLevel) :
		Texture(InTexture),
		MipLevel(InMipLevel)
	{}
};

/** Render graph tracked SRV. */
class RENDERCORE_API FRDGTextureSRV : public FRDGResource
{
public:
	/** Descriptor of the graph tracked SRV. */
	const FRDGTextureSRVDesc Desc;

private:
	FRDGTextureSRV(const TCHAR* DebugName, const FRDGTextureSRVDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	friend class FRDGBuilder;
};

/** Decsriptor for render graph tracked UAV. */
class RENDERCORE_API FRDGTextureUAVDesc
{
public:
	const FRDGTexture* Texture;
	uint8 MipLevel = 0;

	FRDGTextureUAVDesc() {}

	FRDGTextureUAVDesc(const FRDGTexture* InTexture, uint8 InMipLevel = 0) :
		Texture(InTexture),
		MipLevel(InMipLevel)
	{}
};

/** Render graph tracked UAV. */
class RENDERCORE_API FRDGTextureUAV : public FRDGResource
{
public:
	/** Descriptor of the graph tracked UAV. */
	const FRDGTextureUAVDesc Desc;

private:
	FRDGTextureUAV(const TCHAR* DebugName, const FRDGTextureUAVDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	friend class FRDGBuilder;
};


/** Descriptor for render graph tracked Buffer. */
struct RENDERCORE_API FRDGBufferDesc
{
	// Type of buffers to the RHI
	// TODO(RDG): refactor RHI to only have one FRHIBuffer.
	enum class EUnderlyingType
	{
		VertexBuffer,
		IndexBuffer, // not implemented yet.
		StructuredBuffer,
	};

	/** Stride in bytes for index and structured buffers. */
	uint32 BytesPerElement = 1;

	/** Number of elements. */
	uint32 NumElements = 1;

	/** Bitfields describing the uses of that buffer. */
	EBufferUsageFlags Usage = BUF_None;

	/** The underlying RHI type to use. A bit of a work arround because RHI still have 3 different objects. */
	EUnderlyingType UnderlyingType = EUnderlyingType::VertexBuffer;

	/** Returns the total number of bytes allocated for a such buffer. */
	uint32 GetTotalNumBytes() const
	{
		return BytesPerElement * NumElements;
	}


	bool operator == (const FRDGBufferDesc& Other) const
	{
		return (
			BytesPerElement == Other.BytesPerElement &&
			NumElements == Other.NumElements &&
			Usage == Other.Usage &&
			UnderlyingType == Other.UnderlyingType);
	}

	bool operator != (const FRDGBufferDesc& Other) const
	{
		return !(*this == Other);
	}

	/** Create the descriptor for an indirect RHI call.
	 *
	 * Note, IndirectParameterStruct should be one of the:
	 *		struct FRHIDispatchIndirectParameters
	 *		struct FRHIDrawIndirectParameters
	 *		struct FRHIDrawIndexedIndirectParameters
	 */
	template<typename IndirectParameterStruct>
	static inline FRDGBufferDesc CreateIndirectDesc(uint32 NumElements = 1)
	{
		FRDGBufferDesc Desc;
		Desc.UnderlyingType = EUnderlyingType::VertexBuffer;
		Desc.Usage = EBufferUsageFlags(BUF_Static | BUF_DrawIndirect | BUF_UnorderedAccess | BUF_ShaderResource);
		Desc.BytesPerElement = sizeof(IndirectParameterStruct);
		Desc.NumElements = NumElements;
		return Desc;
	}
};


struct RENDERCORE_API FRDGBufferSRVDesc
{
	const FRDGBuffer* Buffer = nullptr;

	/** Nymber of bytes per element (used for vertex buffer). */
	uint32 BytesPerElement = 1;

	/** Encoding format for the element (used for vertex buffer). */
	EPixelFormat Format = PF_Unknown;
};

struct RENDERCORE_API FRDGBufferUAVDesc
{
	const FRDGBuffer* Buffer = nullptr;

	/** Nymber of bytes per element (used for vertex buffer). */
	EPixelFormat Format = PF_Unknown;

	/** Whether the uav supports atomic counter or append buffer operations (used for structured buffers) */
	bool bSupportsAtomicCounter = false;
	bool bSupportsAppendBuffer = false;

	/** Create descriptor for a the UAV of a buffer meant to be use for indirect dral call parameters. */
	static inline FRDGBufferUAVDesc CreateIndirectDesc(const FRDGBuffer* Buffer);
};


/** Defines how the map's pairs are hashed. */
template<typename KeyType, typename ValueType>
struct TMapRDGBufferSRVFuncs : BaseKeyFuncs<TPair<KeyType, ValueType>, KeyType, /* bInAllowDuplicateKeys = */ false>
{
	typedef typename TTypeTraits<KeyType>::ConstPointerType KeyInitType;
	typedef const TPairInitializer<typename TTypeTraits<KeyType>::ConstInitType, typename TTypeTraits<ValueType>::ConstInitType>& ElementInitType;

	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.BytesPerElement == B.BytesPerElement && A.Format == B.Format;
	}
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return HashCombine(uint32(Key.BytesPerElement), uint32(Key.Format));
	}
};

/** Defines how the map's pairs are hashed. */
template<typename KeyType, typename ValueType>
struct TMapRDGBufferUAVFuncs : BaseKeyFuncs<TPair<KeyType, ValueType>, KeyType, /* bInAllowDuplicateKeys = */ false>
{
	typedef typename TTypeTraits<KeyType>::ConstPointerType KeyInitType;
	typedef const TPairInitializer<typename TTypeTraits<KeyType>::ConstInitType, typename TTypeTraits<ValueType>::ConstInitType>& ElementInitType;

	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.Format == B.Format && A.bSupportsAtomicCounter == B.bSupportsAtomicCounter && A.bSupportsAppendBuffer == B.bSupportsAppendBuffer;
	}
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return (uint32(Key.bSupportsAtomicCounter) << 8) | (uint32(Key.bSupportsAppendBuffer) << 9) | uint32(Key.Format);
	}
};

/** Render graph tracked Buffer. Only as opose to vertex, index and structured buffer at RHI level, because there is already
 * plans to refactor them. */
struct FPooledRDGBuffer
{
	FVertexBufferRHIRef VertexBuffer;
	FIndexBufferRHIRef IndexBuffer;
	FStructuredBufferRHIRef StructuredBuffer;
	TMap<FRDGBufferUAVDesc, FUnorderedAccessViewRHIRef, FDefaultSetAllocator, TMapRDGBufferUAVFuncs<FRDGBufferUAVDesc, FUnorderedAccessViewRHIRef>> UAVs;
	TMap<FRDGBufferSRVDesc, FShaderResourceViewRHIRef, FDefaultSetAllocator, TMapRDGBufferSRVFuncs<FRDGBufferSRVDesc, FShaderResourceViewRHIRef>> SRVs;

	/** Descriptor. */
	FRDGBufferDesc Desc;


	// Refcounting
	inline uint32 AddRef()
	{
		return ++RefCount;
	}

	uint32 Release();

	inline uint32 GetRefCount()
	{
		return RefCount;
	}

private:
	uint32 RefCount = 0;
};


/** Render graph tracked buffers. */
class RENDERCORE_API FRDGBuffer : public FRDGResource
{
public:
	/** Descriptor of the graph */
	const FRDGBufferDesc Desc;

	/** Returns the buffer to use for indirect RHI calls. */
	FVertexBufferRHIParamRef GetIndirectRHICallBuffer() const
	{
		check(PooledBuffer);
		checkf(Desc.UnderlyingType == FRDGBufferDesc::EUnderlyingType::VertexBuffer, TEXT("Indirect buffers needs to be underlying vertex buffer."));
		check(PooledBuffer->VertexBuffer.IsValid());
		return PooledBuffer->VertexBuffer;
	}

private:
	/** This is not a TRefCountPtr<> because FRDGBuffer is allocated on the FMemStack
	 * FGraphBuilder::AllocatedBuffers is actually keeping the reference.
	 */
	mutable FPooledRDGBuffer* PooledBuffer = nullptr;

	FRDGBuffer(const TCHAR* DebugName, const FRDGBufferDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	friend class FRDGBuilder;
};

/** Render graph tracked SRV. */
class RENDERCORE_API FRDGBufferSRV : public FRDGResource
{
public:
	/** Descriptor of the graph tracked SRV. */
	const FRDGBufferSRVDesc Desc;

private:
	FRDGBufferSRV(const TCHAR* DebugName, const FRDGBufferSRVDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	friend class FRDGBuilder;
};

/** Render graph tracked UAV. */
class RENDERCORE_API FRDGBufferUAV : public FRDGResource
{
public:
	/** Descriptor of the graph tracked UAV. */
	const FRDGBufferUAVDesc Desc;

private:
	FRDGBufferUAV(const TCHAR* DebugName, const FRDGBufferUAVDesc& InDesc)
		: FRDGResource(DebugName)
		, Desc(InDesc)
	{ }

	friend class FRDGBuilder;
};


FRDGBufferUAVDesc FRDGBufferUAVDesc::CreateIndirectDesc(const FRDGBuffer* Buffer)
{
	checkf(Buffer->Desc.Usage & BUF_DrawIndirect, TEXT("Buffer %s has not been created with a BUF_DrawIndirect flag."), Buffer->Name);
	FRDGBufferUAVDesc Desc;
	Desc.Buffer = Buffer;
	Desc.Format = PF_R32_UINT;
	return Desc;
}


/** Defines the RDG resource references for user code not forgetting the const every time. */
using FRDGResourceRef = const FRDGResource*;
using FRDGTextureRef = const FRDGTexture*;
using FRDGTextureSRVRef = const FRDGTextureSRV*;
using FRDGTextureUAVRef = const FRDGTextureUAV*;
using FRDGBufferRef = const  FRDGBuffer*;
using FRDGBufferSRVRef = const FRDGBufferSRV*;
using FRDGBufferUAVRef = const FRDGBufferUAV*;
