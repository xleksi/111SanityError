// CodeSpartan 2023

#include "ChatGptApiClient.h"

#include "Dialogue.h"
#include "DialogueEditorSubsystem.h"
#include "DialoguePluginEditorSettings.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"


void UChatGptApiClient::Initialize(UDialogue* InDialogue, TArray<int32> Nodes)
{
    Dialogue = InDialogue;
    SelectedNodeIds = Nodes;
    this->AddToRoot();
}

/** Called when the task is complete and can be collected by GC */
void UChatGptApiClient::JobDone()
{
    if (Dialogue.IsValid() && Dialogue->bGenerating)
    {
        Dialogue->bGenerating = false;
    }
    this->RemoveFromRoot();
}

FString UChatGptApiClient::GetModel()
{
    const auto settings = GetDefault<UDialoguePluginEditorSettings>();
    return settings->GetModel();
}

void UChatGptApiClient::StartRequest()
{
	const auto settings = GetDefault<UDialoguePluginEditorSettings>();

    FHttpModule& httpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = httpModule.CreateRequest();

    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *settings->APIKey));

    FString RequestContent = CreateRequestFull();
    HttpRequest->SetContentAsString(RequestContent);

    //UE_LOG(LogTemp, Log, TEXT("Request: %s"), *RequestContent);
    UE_LOG(LogTemp, Log, TEXT("Sending request to ChatGPT"));

    HttpRequest->SetURL(TEXT("https://api.openai.com/v1/chat/completions"));
    HttpRequest->SetTimeout(80.f); // default is 180

    TWeakObjectPtr<UChatGptApiClient> thisObj = this;

    // Set the callback, which will execute when the HTTP call is complete
    HttpRequest->OnProcessRequestComplete().BindLambda(
    [thisObj](FHttpRequestPtr HttpRequestFulfilled, FHttpResponsePtr HttpResponse, bool bSucceeded) mutable
        {
            // the second check is for when we quit PIE, we may get a message about a disconnect, but it's too late to act on it, because the thread has already been killed
            if (!thisObj.IsValid())
                return;
            if (bSucceeded && HttpResponse.IsValid() && EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
            {
                thisObj.Get()->OnResponse(HttpResponse->GetContentAsString(), thisObj);
            }
            else 
            {                
                thisObj.Get()->ResponseFailed(HttpRequestFulfilled);
                // additional info on failure reason:
                UE_LOG(LogTemp, Error, TEXT("Connection succeeded: %s"), bSucceeded ? TEXT("true") : TEXT("false"));
                UE_LOG(LogTemp, Error, TEXT("Http Response is valid: %s"), HttpResponse.IsValid() ? TEXT("true") : TEXT("false"));
                if (HttpResponse.IsValid()) {
                    UE_LOG(LogTemp, Error, TEXT("Http Response: %s"), *HttpResponse->GetContentAsString());
                }
                UE_LOG(LogTemp, Error, TEXT("Http Response Code: %d"), HttpResponse->GetResponseCode());
            }
        }
    );

    HttpRequest->ProcessRequest();
}

void UChatGptApiClient::OnResponse(const FString& ResponseContent, TWeakObjectPtr<UChatGptApiClient> thisObj)
{
    // Ensure this object hasn't gone stale
    if (!thisObj.IsValid())
        return;

    // If we're not on game thread, process this on game thread
    // I don't know if this actually happens, but some tutorial mentioned it's possible
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [&]()
            {
                OnResponse(ResponseContent, thisObj);
            });
        return;
    }

    ProcessResponse(ResponseContent);

    JobDone();
}

void UChatGptApiClient::ResponseFailed(FHttpRequestPtr HttpRequestFulfilled)
{
    EHttpRequestStatus::Type Status = HttpRequestFulfilled->GetStatus();
    switch (Status)
    {
    case EHttpRequestStatus::Failed:
        if (HttpRequestFulfilled->GetFailureReason() == EHttpFailureReason::ConnectionError) {
            UE_LOG(LogTemp, Error, TEXT("Connection failed: connection error"));
        }
        else if (HttpRequestFulfilled->GetFailureReason() == EHttpFailureReason::TimedOut) {
            UE_LOG(LogTemp, Error, TEXT("Connection failed: timed out"));
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("Connection failed: reason unknown"));
        }
    default:
        UE_LOG(LogTemp, Error, TEXT("Request failed."));
    }

    GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));

    JobDone();
}

/**
 * Creates a full request to GPT Api
 * A typical request would be:
    {
        "model": "gpt-3.5-turbo",
        "messages": [
            {
                "role": "system",
                "content": "You are an assistant that helps write dialogues for a computer video game."
            },
            {
                "role": "user",
                "content": "<content here>"
            }
        ],
        "temperature": 1
    }

    Part of the content is created here. (SystemInstruction, general instruction, NPC's disposition, NPC's description)
    Another part of the Content is created in CreateRequestBody(), which informs of the dialogue nodes involved in the request.
 */
FString UChatGptApiClient::CreateRequestFull_Implementation()
{
    const auto settings = GetDefault<UDialoguePluginEditorSettings>();

    FString Result;
    FGptMessage SystemMessage;
    FGptMessage UserMessage;
    FGptRequest Request;

    SystemMessage.role = "system";
    SystemMessage.content = settings->SystemInstruction;

    UserMessage.role = "user";
    UserMessage.content = settings->DialogueInstructions;
    if (!Dialogue->NpcDisposition.IsEmpty() && !Dialogue->NpcDisposition.Compare("Undefined", ESearchCase::IgnoreCase))
    {
        UserMessage.content += "\n";
        UserMessage.content += "NPC's disposition towards the player: " + Dialogue->NpcDisposition;
    }
    if (!Dialogue->Description.IsEmpty())
    {
        UserMessage.content += "\n";
        UserMessage.content += "NPC's description: " + Dialogue->Description.ToString();
    }
    UserMessage.content += CreateRequestBody();

    Request.model = settings->GetModel();
    Request.messages.Add(SystemMessage);
    Request.messages.Add(UserMessage);
    Request.temperature = settings->Temperature;

    FJsonObjectConverter::UStructToJsonObjectString(Request, Result);

    return Result;
}

/*
 * Creates part of the request content that serializes dialogue nodes involved in the request.
 * A typical FString returned by the function would look like:
    {\n  \"dialogueLines\": [\n    {\n      \"id\": 1,\n      \"spokenByPlayer\": false,\n      \"text\": \"(greet)\",\n      \"parentId\": -1\n    },\n    {\n      \"id\": 3,\n      \"spokenByPlayer\": true,\n      \"text\": \"(ask for jobs)\",\n      \"parentId\": 1\n    },\n    {\n      \"id\": 4,\n      \"spokenByPlayer\": true,\n      \"text\": \"(bye)\",\n      \"parentId\": 1\n    },\n    {\n      \"id\": 2,\n      \"spokenByPlayer\": true,\n      \"text\": \"(ask to see wares)\",\n      \"parentId\": 1\n    }\n  ]\n}

    Or the same thing in a more human readable form:
    {
        "dialogueLines":
        [
            {
                "id": 1,
                "spokenByPlayer": false,
                "text": "(greet)",
                "parentId": -1
            },
            {
                "id": 3,
                "spokenByPlayer": true,
                "text": "(ask for jobs)",
                "parentId": 1
            },
            {
                "id": 4,
                "spokenByPlayer": true,
                "text": "(bye)",
                "parentId": 1
            },
            {
                "id": 2,
                "spokenByPlayer": true,
                "text": "(ask to see wares)",
                "parentId": 1
            }
        ]
    }
 */
FString UChatGptApiClient::CreateRequestBody_Implementation()
{
    TArray<FDialogueNode> SelectedNodes;
    for (auto id : SelectedNodeIds)
    {
        int32 index;
        FDialogueNode Node = Dialogue.Get()->GetNodeById(id, index);
        // Special case for the Start node
        if (index == 0)
        {
            FDialogueNode StartNode;
            StartNode.Text = FText::FromString(FString("(this is a utility dialogue line that STARTS the conversation, no text is needed here)"));
            StartNode.id = Node.id;
            StartNode.isPlayer = Node.isPlayer;
            StartNode.Links = Node.Links;
            SelectedNodes.Add(StartNode);
        }
        else
        {
            SelectedNodes.Add(Node);
        }
    }

    FDialogueLines ResultAsStruct;
    ResultAsStruct.DialogueLines = ConvertToDialogueLines(SelectedNodes);

    FString Result;

    FJsonObjectConverter::UStructToJsonObjectString(ResultAsStruct, Result);

    // The function above produces a lot of spammy newline characters and tab characters
    // For example, all messages start as: {\r\n\t\"dialogueLines\": [\r\n\t\t{\r\n\t\t\t\"id\"
    // We're going to clean it out, so it looks like: {\"dialogueLines\": [{\"id\"

    FString Cleaned;
    for (int32 Index = 0; Index < Result.Len(); ++Index)
    {
        TCHAR Character = Result[Index];
        if (Character != TEXT('\r') && Character != TEXT('\n') && Character != TEXT('\t'))
        {
            Cleaned.AppendChar(Character);
        }
    }

    //UE_LOG(LogTemp, Log, TEXT("%s"), *Cleaned);
    return Cleaned;
}

/*
 * Typical Response would be:
	{
	  "id": "chatcmpl-85A3w4JFkDnFkWp8aocY6B6ROgIVs",
	  "object": "chat.completion",
	  "created": 1696241012,
	  "model": "gpt-3.5-turbo-0613",
	  "choices": [
	    {
	      "index": 0,
	      "message": {
	        "role": "assistant",
	        "content": "{\n  \"dialogueLines\": [\n    {\n      \"id\": 1,\n      \"spokenByPlayer\": false,\n      \"text\": \"Greetings! How may I assist you today?\",\n      \"parentId\": -1\n    },\n    {\n      \"id\": 3,\n      \"spokenByPlayer\": true,\n      \"text\": \"Do you have any job openings?\",\n      \"parentId\": 1\n    },\n    {\n      \"id\": 4,\n      \"spokenByPlayer\": true,\n      \"text\": \"Goodbye.\",\n      \"parentId\": 1\n    },\n    {\n      \"id\": 2,\n      \"spokenByPlayer\": true,\n      \"text\": \"May I take a look at your wares?\",\n      \"parentId\": 1\n
	}\n  ]\n}"
	      },
	      "finish_reason": "stop"
	    }
	  ],
	  "usage": {
	    "prompt_tokens": 234,
	    "completion_tokens": 164,
	    "total_tokens": 398
	  }
	}

	Choices[0].message.content contains JSON as an escaped string. We'll process it separately in ProcessContent().
 */
void UChatGptApiClient::ProcessResponse_Implementation(const FString& ResponseContent)
{
    TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ResponseContent);

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    FString GeneratedStats;

    if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
    {
        FString Model;
        if (JsonObject->TryGetStringField(TEXT("model"), Model))
        {
            GeneratedStats += FString::Printf(TEXT("Model: %s"), *Model);
        }
        else
        {
	        // if we can't get the model, it's likely we got bad json or exceeded our quota
            // we can't proceed
            UE_LOG(LogTemp, Warning, TEXT("Response: %s"), *ResponseContent);
            UE_LOG(LogTemp, Warning, TEXT("Unexpected JSON in response. Did you exceed your quota? Inspect the received JSON to see what the issue is."));
            GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
            return;
        }

        auto UsageObject = JsonObject->GetObjectField(TEXT("usage"));
        int32 PromptTokens = UsageObject->GetIntegerField(TEXT("prompt_tokens"));
        int32 CompletionTokens = UsageObject->GetIntegerField(TEXT("completion_tokens"));
        GeneratedStats += FString::Printf(TEXT("\nPrompt Tokens: %d \nCompletion Tokens: %d"), PromptTokens, CompletionTokens);

        auto Choices = JsonObject->GetArrayField(TEXT("Choices"));
        auto Choice0 = Choices[0]->AsObject();
        auto Message = Choice0->GetObjectField(TEXT("message"));
        FString Content = Message->GetStringField(TEXT("content"));

        ProcessContent(Content);
    }
    else
    {
        // if we can't deserialize any json, it's likely we got a bad message or exceeded our quota
        // we can't proceed
        UE_LOG(LogTemp, Warning, TEXT("Response: %s"), *ResponseContent);
        UE_LOG(LogTemp, Warning, TEXT("Unexpected message in response. Did you exceed your quota? Inspect the received response to see what the issue is."));
        GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ChatGPT Stats \n%s"), *GeneratedStats);
}

/*
 * Typical Content would be:
	{
	  "dialogueLines": [
	    {
	      "id": 1,
	      "spokenByPlayer": false,
	      "text": "(greets)",
	      "parentId": -1
	    },
	    {
	      "id": 3,
	      "spokenByPlayer": true,
	      "text": "Do you have any job openings?",
	      "parentId": 1
	    },
	    {
	      "id": 4,
	      "spokenByPlayer": true,
	      "text": "Goodbye",
	      "parentId": 1
	    },
	    {
	      "id": 2,
	      "spokenByPlayer": true,
	      "text": "I would like to view your wares",
	      "parentId": 1
	    }
	  ]
	}
 */
void UChatGptApiClient::ProcessContent_Implementation(const FString& Content)
{
    if (!Dialogue.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to set dialogue lines from GPT reponse. Reason: Dialogue file is not valid."));
        return;
    }

    bool refreshedDetails = false;
    FDialogueLines ResultAsStruct;
    if (FJsonObjectConverter::JsonObjectStringToUStruct(Content, &ResultAsStruct))
    {
        FString ContentFormattedForPrinting;
        FJsonObjectConverter::UStructToJsonObjectString(ResultAsStruct, ContentFormattedForPrinting);
        UE_LOG(LogTemp, Log, TEXT("ChatGPT Response: \n%s"), *ContentFormattedForPrinting);

        GEditor->PlayEditorSound(TEXT("/Engine/VREditor/Sounds/UI/Selection_Changes.Selection_Changes"));

	    for (auto line : ResultAsStruct.DialogueLines)
	    {
            int32 index;
            auto Node = Dialogue->GetNodeById(line.Id, index);

            // don't ever update the Start node
            if (index == 0) continue;

            // GPT sometimes generates more nodes than was required and invents an ID for them.
            // Make sure we only touch nodes that were originally selected.
            if (!SelectedNodeIds.Contains(line.Id))
            {
	            continue;
            }

            // if Node not found, ignore this node (e.g. the user deleted it in the meantime)
            if (index != -1) 
            {
	            // If received text is different
	            // And if original text was either empty or was between parentheses, e.g. (text)
	            if (!Node.Text.ToString().Equals(line.Text) && (Node.Text.IsEmpty() || (Node.Text.ToString().StartsWith(TEXT("(")) && Node.Text.ToString().EndsWith(TEXT(")")))))
	            {
	                const FScopedTransaction Transaction(NSLOCTEXT("ChatGptApiClient", "TextCommitted", "Edited Node Text"));
	                Dialogue->Modify();

                    UDialogueEditorSubsystem* DialogueEditorSubsystem = GEditor->GetEditorSubsystem<UDialogueEditorSubsystem>();
                    DialogueEditorSubsystem->UpdateNodeText(Dialogue.Get(), line.Id, FText::FromString(line.Text));

                    refreshedDetails = true;
	            }
            }
	    }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to deserialize content. Did ChatGPT return malformed content? (if it happens, try making a slightly different request) \n What ChatGPT returned: \n%s"), *Content);
        GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
    }
    if (refreshedDetails)
    {
	    Dialogue->bRefreshDetails = true;
    }
}

TArray<FDialogueLine> UChatGptApiClient::ConvertToDialogueLines(TArray<FDialogueNode>& Nodes)
{
    // Sort from top to bottom
    Nodes.Sort([](const FDialogueNode node1, const FDialogueNode node2) { return node1.Coordinates.Y < node2.Coordinates.Y; });

    TMap<int32, TArray<int32>> parentMapping;

    // First pass: Create a map to store parent-child relationships.
    for (const FDialogueNode& node : Nodes)
    {
        for (int32 childId : node.Links)
        {
            if (parentMapping.Contains(childId))
				parentMapping[childId].Add(node.id);
            else
                parentMapping.Add(childId, TArray<int32>{node.id});
        }
    }

    // Second pass: Build the dialogue lines array.
    TArray<FDialogueLine> dialogueLines;
    for (const FDialogueNode& node : Nodes)
    {
        FDialogueLine line;
        line.Id = node.id;
        line.Text = node.Text.ToString();
        line.SpokenByPlayer = node.isPlayer;

        // Check if the current node id is in the parent mapping.
        if (parentMapping.Contains(node.id) && parentMapping[node.id].Num() > 0)
        {
            // just use the first parent id if there are multiple.
            line.ParentId = parentMapping[node.id][0];
        }
        else
        {
            line.ParentId = -1; // no parent id
        }

        dialogueLines.Add(line);
    }

    return dialogueLines;
}
