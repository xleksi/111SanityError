#pragma once

#include "Editor/PropertyEditor/Public/IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "Animation/CurveSequence.h"

class UDialogue;
class UChatGptApiClient;

class FDialoguePluginEditorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;	

private:
	void TextCommitted(const FText& InText, ETextCommit::Type InCommitType, UDialogue* Dialogue, int32 id);
	void DescriptionCommitted(const FText& InText, ETextCommit::Type InCommitType, UDialogue* Dialogue);
	FReply OnGenerateButtonClicked(UDialogue* Dialogue);
	FText GetGenerateButtonText(UDialogue* Dialogue) const;

	// Throbber related
	// The throbber is mostly taken from SNotificationList.cpp, search for Image(FAppStyle::Get().GetBrush("NotificationList.Throbber"))
	TOptional<FSlateRenderTransform> GetThrobberTransform() const;
	FCurveSequence ThrobberAnimation;
	EVisibility GetThrobberVisibility(UDialogue* Dialogue) const;
	// ~ Throbber related

	/** Associated detail layout builder */
	IDetailLayoutBuilder* DetailLayoutBuilder;
};