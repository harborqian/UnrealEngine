// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpDeserializedMessage.h"

#include "Backends/JsonStructDeserializerBackend.h"
#include "Backends/CborStructDeserializerBackend.h"
#include "StructDeserializer.h"
#include "Serialization/MemoryReader.h"
#include "IMessageAttachment.h"
#include "Transport/UdpReassembledMessage.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UdpMessagingPrivate.h"


/* FUdpDeserializedMessage structors
*****************************************************************************/

FUdpDeserializedMessage::FUdpDeserializedMessage(const TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe>& InAttachment)
	: Attachment(InAttachment)
	, MessageData(nullptr)
	, Flags(EMessageFlags::None)
{ }


FUdpDeserializedMessage::~FUdpDeserializedMessage()
{
	if (MessageData != nullptr)
	{
		if (TypeInfo.IsValid())
		{
			TypeInfo->DestroyStruct(MessageData);
		}

		FMemory::Free(MessageData);
		MessageData = nullptr;
	}
}

/** Helper class for protocol deserialization dispatching */
class FUdpDeserializedMessageDetails
{
public:
	static bool DeserializeV10(FUdpDeserializedMessage& DeserializedMessage, FMemoryReader& MessageReader);
	static bool DeserializeV11(FUdpDeserializedMessage& DeserializedMessage, FMemoryReader& MessageReader);
	static bool Deserialize(FUdpDeserializedMessage& DeserializedMessage, const FUdpReassembledMessage& ReassembledMessage);
};

/* FUdpDeserializedMessage interface
 *****************************************************************************/

bool FUdpDeserializedMessage::Deserialize(const FUdpReassembledMessage& ReassembledMessage)
{
	return FUdpDeserializedMessageDetails::Deserialize(*this, ReassembledMessage);
}

/* IMessageContext interface
 *****************************************************************************/

const TMap<FName, FString>& FUdpDeserializedMessage::GetAnnotations() const
{
	return Annotations;
}


TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> FUdpDeserializedMessage::GetAttachment() const
{
	return Attachment;
}


const FDateTime& FUdpDeserializedMessage::GetExpiration() const
{
	return Expiration;
}


const void* FUdpDeserializedMessage::GetMessage() const
{
	return MessageData;
}


const TWeakObjectPtr<UScriptStruct>& FUdpDeserializedMessage::GetMessageTypeInfo() const
{
	return TypeInfo;
}


TSharedPtr<IMessageContext, ESPMode::ThreadSafe> FUdpDeserializedMessage::GetOriginalContext() const
{
	return nullptr;
}


const TArray<FMessageAddress>& FUdpDeserializedMessage::GetRecipients() const
{
	return Recipients;
}


EMessageScope FUdpDeserializedMessage::GetScope() const
{
	return Scope;
}


EMessageFlags FUdpDeserializedMessage::GetFlags() const
{
	return Flags;
}


const FMessageAddress& FUdpDeserializedMessage::GetSender() const
{
	return Sender;
}


ENamedThreads::Type FUdpDeserializedMessage::GetSenderThread() const
{
	return ENamedThreads::AnyThread;
}


const FDateTime& FUdpDeserializedMessage::GetTimeForwarded() const
{
	return TimeSent;
}


const FDateTime& FUdpDeserializedMessage::GetTimeSent() const
{
	return TimeSent;
}


/* FUdpDeserializedMessageDetails implementation
*****************************************************************************/


bool FUdpDeserializedMessageDetails::DeserializeV10(FUdpDeserializedMessage& DeserializedMessage, FMemoryReader& MessageReader)
{
	// message type info
	{
		FName MessageType;
		MessageReader << MessageType;

		// @todo gmp: cache message types for faster lookup
		DeserializedMessage.TypeInfo = FindObjectSafe<UScriptStruct>(ANY_PACKAGE, *MessageType.ToString());

		if (!DeserializedMessage.TypeInfo.IsValid(false, true))
		{
			return false;
		}
	}

	// sender address
	{
		MessageReader << DeserializedMessage.Sender;
	}

	// recipient addresses
	{
		int32 NumRecipients = 0;
		MessageReader << NumRecipients;

		if ((NumRecipients < 0) || (NumRecipients > UDP_MESSAGING_MAX_RECIPIENTS))
		{
			return false;
		}

		DeserializedMessage.Recipients.Empty(NumRecipients);

		while (0 < NumRecipients--)
		{
			MessageReader << DeserializedMessage.Recipients.AddDefaulted_GetRef();
		}
	}

	// message scope
	{
		MessageReader << DeserializedMessage.Scope;

		if (DeserializedMessage.Scope > EMessageScope::All)
		{
			return false;
		}
	}

	// time sent & expiration
	{
		MessageReader << DeserializedMessage.TimeSent;
		MessageReader << DeserializedMessage.Expiration;
	}

	// annotations
	{
		int32 NumAnnotations = 0;
		MessageReader << NumAnnotations;

		if (NumAnnotations > UDP_MESSAGING_MAX_ANNOTATIONS)
		{
			return false;
		}

		while (0 < NumAnnotations--)
		{
			FName Key;
			FString Value;

			MessageReader << Key;
			MessageReader << Value;

			DeserializedMessage.Annotations.Add(Key, Value);
		}
	}

	// create message body
	DeserializedMessage.MessageData = FMemory::Malloc(DeserializedMessage.TypeInfo->GetStructureSize());
	DeserializedMessage.TypeInfo->InitializeStruct(DeserializedMessage.MessageData);

	// deserialize message body
	FJsonStructDeserializerBackend Backend(MessageReader);
	return FStructDeserializer::Deserialize(DeserializedMessage.MessageData, *DeserializedMessage.TypeInfo, Backend);
}

bool FUdpDeserializedMessageDetails::DeserializeV11(FUdpDeserializedMessage& DeserializedMessage, FMemoryReader& MessageReader)
{
	// message type info
	{
		FName MessageType;
		MessageReader << MessageType;

		// @todo gmp: cache message types for faster lookup
		DeserializedMessage.TypeInfo = FindObjectSafe<UScriptStruct>(ANY_PACKAGE, *MessageType.ToString());

		if (!DeserializedMessage.TypeInfo.IsValid(false, true))
		{
			return false;
		}
	}

	// sender address
	{
		MessageReader << DeserializedMessage.Sender;
	}

	// recipient addresses
	{
		int32 NumRecipients = 0;
		MessageReader << NumRecipients;

		if ((NumRecipients < 0) || (NumRecipients > UDP_MESSAGING_MAX_RECIPIENTS))
		{
			return false;
		}

		DeserializedMessage.Recipients.Empty(NumRecipients);

		while (0 < NumRecipients--)
		{
			MessageReader << DeserializedMessage.Recipients.AddDefaulted_GetRef();
		}
	}

	// message scope
	{
		MessageReader << DeserializedMessage.Scope;

		if (DeserializedMessage.Scope > EMessageScope::All)
		{
			return false;
		}
	}

	// message flags
	{
		MessageReader << DeserializedMessage.Flags;
	}

	// time sent & expiration
	{
		MessageReader << DeserializedMessage.TimeSent;
		MessageReader << DeserializedMessage.Expiration;
	}

	// annotations
	{
		int32 NumAnnotations = 0;
		MessageReader << NumAnnotations;

		if (NumAnnotations > UDP_MESSAGING_MAX_ANNOTATIONS)
		{
			return false;
		}

		while (0 < NumAnnotations--)
		{
			FName Key;
			FString Value;

			MessageReader << Key;
			MessageReader << Value;

			DeserializedMessage.Annotations.Add(Key, Value);
		}
	}

	// wire format 
	uint8 FormatId;
	MessageReader << FormatId;
	EUdpMessageFormat MessageFormat = (EUdpMessageFormat)FormatId;
	
	// create message body
	DeserializedMessage.MessageData = FMemory::Malloc(DeserializedMessage.TypeInfo->GetStructureSize());
	DeserializedMessage.TypeInfo->InitializeStruct(DeserializedMessage.MessageData);

	switch (MessageFormat)
	{
	case EUdpMessageFormat::Json:
	{
		// deserialize json
		FJsonStructDeserializerBackend Backend(MessageReader);
		return FStructDeserializer::Deserialize(DeserializedMessage.MessageData, *DeserializedMessage.TypeInfo, Backend);
	}
	break;
	case EUdpMessageFormat::Cbor:
	{
		// deserialize cbor
		FCborStructDeserializerBackend Backend(MessageReader);
		return FStructDeserializer::Deserialize(DeserializedMessage.MessageData, *DeserializedMessage.TypeInfo, Backend);
	}
	break;
	case EUdpMessageFormat::TaggedProperty:
	{
		// deserialize message body using tagged property
		// Hack : this binary serialization should use a more standard protocol, should use cbor
		DeserializedMessage.TypeInfo->SerializeItem(MessageReader, DeserializedMessage.MessageData, nullptr);
		return !MessageReader.GetError();
	}
	break;
	default:
		// Unsupported format
		return false;
	}
}

bool FUdpDeserializedMessageDetails::Deserialize(FUdpDeserializedMessage& DeserializedMessage, const FUdpReassembledMessage& ReassembledMessage)
{
	// Note that some complex values are deserialized manually here, so that we
	// can sanity check their values. @see FUdpSerializeMessageTask::DoTask()

	FMemoryReader MessageReader(ReassembledMessage.GetData());
	MessageReader.ArMaxSerializeSize = NAME_SIZE;

	switch (ReassembledMessage.GetProtocolVersion())
	{
	case 10:
		return DeserializeV10(DeserializedMessage, MessageReader);
		break;

	case 11:
		return DeserializeV11(DeserializedMessage, MessageReader);
		break;

	default:
		UE_LOG(LogUdpMessaging, Error, TEXT("Unsupported Protocol Version message tasked for deserialization, discarding..."));
		break;
	}
	return false;
}

