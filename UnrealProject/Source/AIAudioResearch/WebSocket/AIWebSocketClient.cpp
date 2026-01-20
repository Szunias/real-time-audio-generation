// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#include "AIWebSocketClient.h"
#include "WebSocketsModule.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FAIWebSocketClient::FAIWebSocketClient()
	: bIsConnected(false)
	, LastRequestTimestamp(0.0)
{
}

FAIWebSocketClient::~FAIWebSocketClient()
{
	Close();
}

void FAIWebSocketClient::Connect(const FString& URL)
{
	// Ensure WebSockets module is loaded
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}
	
	// Create WebSocket
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(URL, TEXT("ws"));
	
	if (!WebSocket.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("AudioWebSocket: Failed to create WebSocket"));
		return;
	}
	
	// Bind events using lambdas
	WebSocket->OnConnected().AddLambda([this]()
	{
		OnConnectedInternal();
	});
	
	WebSocket->OnConnectionError().AddLambda([this](const FString& Error)
	{
		OnConnectionError(Error);
	});
	
	WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		OnClosed(StatusCode, Reason, bWasClean);
	});
	
	WebSocket->OnMessage().AddLambda([this](const FString& Message)
	{
		OnMessageReceived(Message);
	});
	
	WebSocket->OnRawMessage().AddLambda([this](const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
	{
		OnBinaryMessageReceived(Data, Size, BytesRemaining);
	});
	
	// Connect
	UE_LOG(LogTemp, Log, TEXT("AudioWebSocket: Connecting to %s"), *URL);
	WebSocket->Connect();
}

void FAIWebSocketClient::Close()
{
	// Set flag FIRST using atomic store for thread safety
	bIsClosing.store(true);
	
	if (WebSocket.IsValid())
	{
		// CRITICAL: Unbind ALL delegates BEFORE closing to prevent callbacks
		WebSocket->OnConnected().Clear();
		WebSocket->OnConnectionError().Clear();
		WebSocket->OnClosed().Clear();
		WebSocket->OnMessage().Clear();
		WebSocket->OnRawMessage().Clear();
		
		if (bIsConnected)
		{
			WebSocket->Close();
		}
		WebSocket.Reset();
	}
	bIsConnected = false;
	// Safe to clear buffer now since no callbacks can fire
	BinaryBuffer.Empty();
}

void FAIWebSocketClient::RequestSound(const FString& Prompt, const FString& Model, float Duration)
{
	if (!IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("AudioWebSocket: Cannot send request - not connected"));
		return;
	}
	
	// Clear buffer for new request
	BinaryBuffer.Empty();
	
	// Record timestamp for latency measurement
	LastRequestTimestamp = FPlatformTime::Seconds();
	
	// Create JSON request for unified server
	// Format: {"model": "audiogen", "prompt": "footstep", "duration": 2.0}
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("model"), Model);
	JsonObject->SetStringField(TEXT("prompt"), Prompt);
	JsonObject->SetNumberField(TEXT("duration"), Duration);
	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	UE_LOG(LogTemp, Log, TEXT("AudioWebSocket: Requesting sound - model:%s prompt:%s (%.1fs)"), *Model, *Prompt, Duration);
	WebSocket->Send(OutputString);
}

bool FAIWebSocketClient::IsConnected() const
{
	return bIsConnected && WebSocket.IsValid() && WebSocket->IsConnected();
}

void FAIWebSocketClient::OnConnectedInternal()
{
	if (bIsClosing.load()) return;
	
	bIsConnected = true;
	UE_LOG(LogTemp, Log, TEXT("AudioWebSocket: Connected successfully"));
	OnConnected.ExecuteIfBound();
}

void FAIWebSocketClient::OnConnectionError(const FString& Error)
{
	if (bIsClosing.load()) return;
	
	bIsConnected = false;
	UE_LOG(LogTemp, Error, TEXT("AudioWebSocket: Connection error - %s"), *Error);
	OnError.ExecuteIfBound(Error);
}

void FAIWebSocketClient::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	// CRITICAL: Check flag to prevent crash during destruction
	if (bIsClosing.load()) return;
	
	bIsConnected = false;
	BinaryBuffer.Empty();
	UE_LOG(LogTemp, Log, TEXT("AudioWebSocket: Connection closed - Code: %d, Reason: %s"), StatusCode, *Reason);
}

void FAIWebSocketClient::OnMessageReceived(const FString& Message)
{
	UE_LOG(LogTemp, Log, TEXT("AudioWebSocket: Received text message - %s"), *Message);
	
	// Parse JSON response for metadata
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		// Handle metadata messages (e.g., latency info, errors)
		if (JsonObject->HasField(TEXT("error")))
		{
			FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
			UE_LOG(LogTemp, Error, TEXT("AudioWebSocket: Server error - %s"), *ErrorMsg);
			OnError.ExecuteIfBound(ErrorMsg);
		}
	}
}

void FAIWebSocketClient::OnBinaryMessageReceived(const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
{
	// CRITICAL: Check flag to prevent crash during destruction
	if (bIsClosing.load()) return;
	
	// Append data to buffer
	int32 OldSize = BinaryBuffer.Num();
	BinaryBuffer.SetNumUninitialized(OldSize + Size);
	FMemory::Memcpy(BinaryBuffer.GetData() + OldSize, Data, Size);
	
	UE_LOG(LogTemp, Verbose, TEXT("AudioWebSocket: Received fragment %lld bytes, %lld remaining, buffer now %d bytes"), 
		(long long)Size, (long long)BytesRemaining, BinaryBuffer.Num());
	
	// When BytesRemaining is 0, we have the complete message
	if (BytesRemaining == 0 && BinaryBuffer.Num() > 0)
	{
		double ReceiveTimestamp = FPlatformTime::Seconds();
		double Latency = (ReceiveTimestamp - LastRequestTimestamp) * 1000.0; // Convert to ms
		
		UE_LOG(LogTemp, Log, TEXT("AudioWebSocket: Received complete audio data - %d bytes, Latency: %.2f ms"), 
			BinaryBuffer.Num(), Latency);
		
		// Execute delegate with complete data
		OnAudioDataReceived.ExecuteIfBound(BinaryBuffer);
		
		// Clear buffer for next message
		BinaryBuffer.Empty();
	}
}
