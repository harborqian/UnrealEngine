// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>

#pragma clang diagnostic ignored "-Wnullability-completeness"

@class FMetalDebugCommandEncoder;

@interface FMetalDebugFence : FApplePlatformObject<MTLFence>
{
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> UpdatingEncoders;
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> WaitingEncoders;
	NSString* Label;
}
@property (retain) id<MTLFence> Inner;
-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders;
-(void)validate;
@end

@protocol MTLDeviceExtensions <MTLDevice>
/*!
 @method newFence
 @abstract Create a new MTLFence object
 */
- (id <MTLFence>)newFence;
@end

@protocol MTLBlitCommandEncoderExtensions <MTLBlitCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLComputeCommandEncoderExtensions <MTLComputeCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLRenderCommandEncoderExtensions <MTLRenderCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder for the given
 stages.
 @discussion Unlike <st>updateFence:</st>, this method will update the event when the given stage(s) complete, allowing for commands to overlap in execution.
 */
-(void) updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages;
/*!
 @abstract Prevent further GPU work until the event is reached for the given stages.
 @discussion Unlike <st>waitForFence:</st>, this method will only block commands assoicated with the given stage(s), allowing for commands to overlap in execution.
 */
-(void) waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages;
@end

class FMetalFence : public mtlpp::Fence
{
	enum
	{
		NumFenceStages = 2
	};
public:
	FMetalFence()
	: mtlpp::Fence(nil)
	{
		for (uint32 i = 0; i < NumFenceStages; i++)
		{
			Writes[i] = 0;
			Waits[i] = 0;
		}
	}
	
	explicit FMetalFence(mtlpp::Fence Obj)
	: mtlpp::Fence(Obj)
	{
		for (uint32 i = 0; i < NumFenceStages; i++)
		{
			Writes[i] = 0;
			Waits[i] = 0;
		}
	}
	
	explicit FMetalFence(FMetalFence const& Other)
	{
		operator=(Other);
	}
	
	~FMetalFence()
	{
	}
	
	FMetalFence& operator=(FMetalFence const& Other)
	{
		if (&Other != this)
		{
			mtlpp::Fence::operator=(Other);
		}
		return *this;
	}
	
	FMetalFence& operator=(mtlpp::Fence const& Other)
	{
		if (Other.GetPtr() != GetPtr())
		{
			METAL_DEBUG_OPTION(Validate());
			mtlpp::Fence::operator=(Other);
		}
		return *this;
	}
	
	FMetalFence& operator=(mtlpp::Fence&& Other)
	{
		if (Other.GetPtr() != GetPtr())
		{
			METAL_DEBUG_OPTION(Validate());
			mtlpp::Fence::operator=(Other);
		}
		return *this;
	}
	
#if METAL_DEBUG_OPTIONS
	void Validate(void) const;
#endif
	
	void Reset(void)
	{
		for (uint32 i = 0; i < NumFenceStages; i++)
		{
			Writes[i] = 0;
			Waits[i] = 0;
		}
	}
	
	void Write(mtlpp::RenderStages Stage)
	{
		Writes[(uint32)Stage - 1u]++;
	}
	
	void Wait(mtlpp::RenderStages Stage)
	{
		Waits[(uint32)Stage - 1u]++;
	}
	
	int8 NumWrites(mtlpp::RenderStages Stage) const
	{
		return Writes[(uint32)Stage - 1u];
	}
	
	int8 NumWaits(mtlpp::RenderStages Stage) const
	{
		return Waits[(uint32)Stage - 1u];
	}
	
	bool NeedsWrite(mtlpp::RenderStages Stage) const
	{
		return Writes[(uint32)Stage - 1u] == 0 || (Waits[(uint32)Stage - 1u] > Writes[(uint32)Stage - 1u]);
	}
	
	bool NeedsWait(mtlpp::RenderStages Stage) const
	{
		return Waits[(uint32)Stage - 1u] == 0 || (Writes[(uint32)Stage - 1u] > Waits[(uint32)Stage - 1u]);
	}
	
	static void ValidateUsage(FMetalFence* Fence);
	
private:
	int8 Writes[NumFenceStages];
	int8 Waits[NumFenceStages];
};

class FMetalFencePool
{
	enum
	{
		NumFences = 2048
	};
public:
	FMetalFencePool() {}
	
	static FMetalFencePool& Get()
	{
		return sSelf;
	}
	
	void Initialise(mtlpp::Device const& InDevice);
	
	FMetalFence* AllocateFence();
	void ReleaseFence(FMetalFence* const InFence);
	
private:
	int32 Count;
	mtlpp::Device Device;
	TSet<FMetalFence*> Fences;
	FCriticalSection Mutex;
	TLockFreePointerListLIFO<FMetalFence> Lifo;
	static FMetalFencePool sSelf;
};
