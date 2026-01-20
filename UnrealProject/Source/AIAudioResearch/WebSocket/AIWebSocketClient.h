// Copyright 2025 Igor Szuniewicz. Audio Generation Research Project.

#pragma once

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include <atomic>

// Native C++ delegates (not Blueprint-compatible but work in non-UObject classes)
DECLARE_DELEGATE_OneParam(FOnAudioDataReceivedNative, const TArray<uint8>&);
DECLARE_DELEGATE(FOnWebSocketConnectedNative);
DECLARE_DELEGATE_OneParam(FOnWebSocketErrorNative, const FString&);

/**
 * WebSocket client for communicating with Python AI audio backend
 * Uses native C++ delegates for event handling
 * Properly buffers fragmented binary messages
 */
class AIAUDIORESEARCH_API FAudioWebSocketClient : public TSharedFromThis<FAudioWebSocketClient>
{
public:
	FAudioWebSocketClient();
	~FAudioWebSocketClient();

	/** Connect to the Python backend server */
	void Connect(const FString& URL = TEXT("ws://localhost:8765"));
	
	/** Disconnect from server */
	void Close();
	
	/** Send a sound generation request */
	void RequestSound(const FString& Prompt, const FString& Model = TEXT("procedural"), float Duration = 2.0f);
	
	/** Check if connected */
	bool IsConnected() const;

	/** Native C++ delegates - bind with BindRaw, BindUObject, or BindLambda */
	FOnAudioDataReceivedNative OnAudioDataReceived;
	FOnWebSocketConnectedNative OnConnected;
	FOnWebSocketErrorNative OnError;
	
	/** Get last request timestamp (for latency measurement) */
	double GetLastRequestTimestamp() const { return LastRequestTimestamp; }

private:
	void OnMessageReceived(const FString& Message);
	void OnBinaryMessageReceived(const void* Data, SIZE_T Size, SIZE_T BytesRemaining);
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnConnectedInternal();

	TSharedPtr<IWebSocket> WebSocket;
	bool bIsConnected;
	std::atomic<bool> bIsClosing{false};  // Atomic for thread-safe callback blocking
	
	/** Timestamp when last request was sent (for latency measurement) */
	double LastRequestTimestamp;
	
	/** Buffer for accumulating fragmented binary data */
	TArray<uint8> BinaryBuffer;
};
