
type Content = string
type ProtectedString = string
type BinaryString = string
type QDir = string
type QFont = string
type FloatCurveKey = any
type RotationCurveKey = any

declare class Enum
    function GetEnumItems(self): { any }
end

declare class EnumItem
    Name: string
    Value: number
    EnumType: Enum
    function IsA(self, enumName: string): boolean
end

declare debug: {
    info: (<R...>(thread, number, string) -> R...) & (<R...>(number, string) -> R...) & (<A..., R1..., R2...>((A...) -> R1..., string) -> R2...),
    traceback: ((string?, number?) -> string) & ((thread, string?, number?) -> string),
    profilebegin: (label: string) -> (),
    profileend: () -> (),
    setmemorycategory: (tag: string) -> (),
    resetmemorycategory: () -> (),
}

declare task: {
    cancel: (thread: thread) -> (),
    defer: <A..., R...>(f: thread | ((A...) -> R...), A...) -> thread,
    spawn: <A..., R...>(f: thread | ((A...) -> R...), A...) -> thread,
    delay: <A..., R...>(sec: number?, f: thread | ((A...) -> R...), A...) -> thread,
    wait: (sec: number?) -> number,
    synchronize: () -> (),
    desynchronize: () -> (),
}

declare utf8: {
    char: (...number) -> string,
    charpattern: string,
    codepoint: (string, number?, number?) -> (...number),
    codes: (string) -> ((string, number) -> (number, number), string, number),
    graphemes: (string, number?, number?) -> (() -> (number, number)),
    len: (string, number?, number?) -> (number?, number?),
    nfcnormalize: (string) -> string,
    nfdnormalize: (string) -> string,
    offset: (string, number, number?) -> number?,
}

declare shared: any

declare function collectgarbage(mode: "count"): number
declare function warn<T...>(...: T...)
declare function tick(): number
declare function time(): number
declare function elapsedTime(): number
declare function wait(seconds: number?): (number, number)
declare function delay<T...>(delayTime: number?, callback: (T...) -> ())
declare function spawn<T...>(callback: (T...) -> ())
declare function version(): string
declare function printidentity(prefix: string?)

declare class EnumAccessoryType extends EnumItem end
declare class EnumAccessoryType_INTERNAL extends Enum
	Unknown: EnumAccessoryType
	Hat: EnumAccessoryType
	Hair: EnumAccessoryType
	Face: EnumAccessoryType
	Neck: EnumAccessoryType
	Shoulder: EnumAccessoryType
	Front: EnumAccessoryType
	Back: EnumAccessoryType
	Waist: EnumAccessoryType
	TShirt: EnumAccessoryType
	Shirt: EnumAccessoryType
	Pants: EnumAccessoryType
	Jacket: EnumAccessoryType
	Sweater: EnumAccessoryType
	Shorts: EnumAccessoryType
	LeftShoe: EnumAccessoryType
	RightShoe: EnumAccessoryType
	DressSkirt: EnumAccessoryType
	Eyebrow: EnumAccessoryType
	Eyelash: EnumAccessoryType
end
declare class EnumActionType extends EnumItem end
declare class EnumActionType_INTERNAL extends Enum
	Nothing: EnumActionType
	Pause: EnumActionType
	Lose: EnumActionType
	Draw: EnumActionType
	Win: EnumActionType
end
declare class EnumActuatorRelativeTo extends EnumItem end
declare class EnumActuatorRelativeTo_INTERNAL extends Enum
	Attachment0: EnumActuatorRelativeTo
	Attachment1: EnumActuatorRelativeTo
	World: EnumActuatorRelativeTo
end
declare class EnumActuatorType extends EnumItem end
declare class EnumActuatorType_INTERNAL extends Enum
	None: EnumActuatorType
	Motor: EnumActuatorType
	Servo: EnumActuatorType
end
declare class EnumAdShape extends EnumItem end
declare class EnumAdShape_INTERNAL extends Enum
	HorizontalRectangle: EnumAdShape
end
declare class EnumAdTeleportMethod extends EnumItem end
declare class EnumAdTeleportMethod_INTERNAL extends Enum
	Undefined: EnumAdTeleportMethod
	PortalForward: EnumAdTeleportMethod
	InGameMenuBackButton: EnumAdTeleportMethod
	UIBackButton: EnumAdTeleportMethod
end
declare class EnumAdUnitStatus extends EnumItem end
declare class EnumAdUnitStatus_INTERNAL extends Enum
	Inactive: EnumAdUnitStatus
	Active: EnumAdUnitStatus
end
declare class EnumAdornCullingMode extends EnumItem end
declare class EnumAdornCullingMode_INTERNAL extends Enum
	Automatic: EnumAdornCullingMode
	Never: EnumAdornCullingMode
end
declare class EnumAlignType extends EnumItem end
declare class EnumAlignType_INTERNAL extends Enum
	Parallel: EnumAlignType
	Perpendicular: EnumAlignType
end
declare class EnumAlphaMode extends EnumItem end
declare class EnumAlphaMode_INTERNAL extends Enum
	Overlay: EnumAlphaMode
	Transparency: EnumAlphaMode
end
declare class EnumAnalyticsEconomyAction extends EnumItem end
declare class EnumAnalyticsEconomyAction_INTERNAL extends Enum
	Default: EnumAnalyticsEconomyAction
	Acquire: EnumAnalyticsEconomyAction
	Spend: EnumAnalyticsEconomyAction
end
declare class EnumAnalyticsLogLevel extends EnumItem end
declare class EnumAnalyticsLogLevel_INTERNAL extends Enum
	Trace: EnumAnalyticsLogLevel
	Debug: EnumAnalyticsLogLevel
	Information: EnumAnalyticsLogLevel
	Warning: EnumAnalyticsLogLevel
	Error: EnumAnalyticsLogLevel
	Fatal: EnumAnalyticsLogLevel
end
declare class EnumAnalyticsProgressionStatus extends EnumItem end
declare class EnumAnalyticsProgressionStatus_INTERNAL extends Enum
	Default: EnumAnalyticsProgressionStatus
	Begin: EnumAnalyticsProgressionStatus
	Complete: EnumAnalyticsProgressionStatus
	Abandon: EnumAnalyticsProgressionStatus
	Fail: EnumAnalyticsProgressionStatus
end
declare class EnumAnimationPriority extends EnumItem end
declare class EnumAnimationPriority_INTERNAL extends Enum
	Idle: EnumAnimationPriority
	Movement: EnumAnimationPriority
	Action: EnumAnimationPriority
	Action2: EnumAnimationPriority
	Action3: EnumAnimationPriority
	Action4: EnumAnimationPriority
	Core: EnumAnimationPriority
end
declare class EnumAnimatorRetargetingMode extends EnumItem end
declare class EnumAnimatorRetargetingMode_INTERNAL extends Enum
	Default: EnumAnimatorRetargetingMode
	Disabled: EnumAnimatorRetargetingMode
	Enabled: EnumAnimatorRetargetingMode
end
declare class EnumAppShellActionType extends EnumItem end
declare class EnumAppShellActionType_INTERNAL extends Enum
	None: EnumAppShellActionType
	OpenApp: EnumAppShellActionType
	TapChatTab: EnumAppShellActionType
	TapConversationEntry: EnumAppShellActionType
	TapAvatarTab: EnumAppShellActionType
	ReadConversation: EnumAppShellActionType
	TapGamePageTab: EnumAppShellActionType
	TapHomePageTab: EnumAppShellActionType
	GamePageLoaded: EnumAppShellActionType
	HomePageLoaded: EnumAppShellActionType
	AvatarEditorPageLoaded: EnumAppShellActionType
end
declare class EnumAppShellFeature extends EnumItem end
declare class EnumAppShellFeature_INTERNAL extends Enum
	None: EnumAppShellFeature
	Chat: EnumAppShellFeature
	AvatarEditor: EnumAppShellFeature
	GamePage: EnumAppShellFeature
	HomePage: EnumAppShellFeature
	More: EnumAppShellFeature
	Landing: EnumAppShellFeature
end
declare class EnumAppUpdateStatus extends EnumItem end
declare class EnumAppUpdateStatus_INTERNAL extends Enum
	Unknown: EnumAppUpdateStatus
	NotSupported: EnumAppUpdateStatus
	Failed: EnumAppUpdateStatus
	NotAvailable: EnumAppUpdateStatus
	Available: EnumAppUpdateStatus
end
declare class EnumApplyStrokeMode extends EnumItem end
declare class EnumApplyStrokeMode_INTERNAL extends Enum
	Contextual: EnumApplyStrokeMode
	Border: EnumApplyStrokeMode
end
declare class EnumAspectType extends EnumItem end
declare class EnumAspectType_INTERNAL extends Enum
	FitWithinMaxSize: EnumAspectType
	ScaleWithParentSize: EnumAspectType
end
declare class EnumAssetFetchStatus extends EnumItem end
declare class EnumAssetFetchStatus_INTERNAL extends Enum
	Success: EnumAssetFetchStatus
	Failure: EnumAssetFetchStatus
	None: EnumAssetFetchStatus
	Loading: EnumAssetFetchStatus
	TimedOut: EnumAssetFetchStatus
end
declare class EnumAssetType extends EnumItem end
declare class EnumAssetType_INTERNAL extends Enum
	Image: EnumAssetType
	TShirt: EnumAssetType
	Audio: EnumAssetType
	Mesh: EnumAssetType
	Lua: EnumAssetType
	Hat: EnumAssetType
	Place: EnumAssetType
	Model: EnumAssetType
	Shirt: EnumAssetType
	Pants: EnumAssetType
	Decal: EnumAssetType
	Head: EnumAssetType
	Face: EnumAssetType
	Gear: EnumAssetType
	Badge: EnumAssetType
	Animation: EnumAssetType
	Torso: EnumAssetType
	RightArm: EnumAssetType
	LeftArm: EnumAssetType
	LeftLeg: EnumAssetType
	RightLeg: EnumAssetType
	Package: EnumAssetType
	GamePass: EnumAssetType
	Plugin: EnumAssetType
	MeshPart: EnumAssetType
	HairAccessory: EnumAssetType
	FaceAccessory: EnumAssetType
	NeckAccessory: EnumAssetType
	ShoulderAccessory: EnumAssetType
	FrontAccessory: EnumAssetType
	BackAccessory: EnumAssetType
	WaistAccessory: EnumAssetType
	ClimbAnimation: EnumAssetType
	DeathAnimation: EnumAssetType
	FallAnimation: EnumAssetType
	IdleAnimation: EnumAssetType
	JumpAnimation: EnumAssetType
	RunAnimation: EnumAssetType
	SwimAnimation: EnumAssetType
	WalkAnimation: EnumAssetType
	PoseAnimation: EnumAssetType
	MoodAnimation: EnumAssetType
	EarAccessory: EnumAssetType
	EyeAccessory: EnumAssetType
	EmoteAnimation: EnumAssetType
	Video: EnumAssetType
	TShirtAccessory: EnumAssetType
	ShirtAccessory: EnumAssetType
	PantsAccessory: EnumAssetType
	JacketAccessory: EnumAssetType
	SweaterAccessory: EnumAssetType
	ShortsAccessory: EnumAssetType
	LeftShoeAccessory: EnumAssetType
	RightShoeAccessory: EnumAssetType
	DressSkirtAccessory: EnumAssetType
	EyebrowAccessory: EnumAssetType
	EyelashAccessory: EnumAssetType
	DynamicHead: EnumAssetType
	FontFamily: EnumAssetType
end
declare class EnumAssetTypeVerification extends EnumItem end
declare class EnumAssetTypeVerification_INTERNAL extends Enum
	Default: EnumAssetTypeVerification
	ClientOnly: EnumAssetTypeVerification
	Always: EnumAssetTypeVerification
end
declare class EnumAudioSubType extends EnumItem end
declare class EnumAudioSubType_INTERNAL extends Enum
	Music: EnumAudioSubType
	SoundEffect: EnumAudioSubType
end
declare class EnumAutoIndentRule extends EnumItem end
declare class EnumAutoIndentRule_INTERNAL extends Enum
	Off: EnumAutoIndentRule
	Absolute: EnumAutoIndentRule
	Relative: EnumAutoIndentRule
end
declare class EnumAutomaticSize extends EnumItem end
declare class EnumAutomaticSize_INTERNAL extends Enum
	None: EnumAutomaticSize
	X: EnumAutomaticSize
	Y: EnumAutomaticSize
	XY: EnumAutomaticSize
end
declare class EnumAvatarAssetType extends EnumItem end
declare class EnumAvatarAssetType_INTERNAL extends Enum
	TShirt: EnumAvatarAssetType
	Hat: EnumAvatarAssetType
	HairAccessory: EnumAvatarAssetType
	FaceAccessory: EnumAvatarAssetType
	NeckAccessory: EnumAvatarAssetType
	ShoulderAccessory: EnumAvatarAssetType
	FrontAccessory: EnumAvatarAssetType
	BackAccessory: EnumAvatarAssetType
	WaistAccessory: EnumAvatarAssetType
	Shirt: EnumAvatarAssetType
	Pants: EnumAvatarAssetType
	Gear: EnumAvatarAssetType
	Head: EnumAvatarAssetType
	Face: EnumAvatarAssetType
	Torso: EnumAvatarAssetType
	RightArm: EnumAvatarAssetType
	LeftArm: EnumAvatarAssetType
	LeftLeg: EnumAvatarAssetType
	RightLeg: EnumAvatarAssetType
	ClimbAnimation: EnumAvatarAssetType
	FallAnimation: EnumAvatarAssetType
	IdleAnimation: EnumAvatarAssetType
	JumpAnimation: EnumAvatarAssetType
	RunAnimation: EnumAvatarAssetType
	SwimAnimation: EnumAvatarAssetType
	WalkAnimation: EnumAvatarAssetType
	MoodAnimation: EnumAvatarAssetType
	EmoteAnimation: EnumAvatarAssetType
	TShirtAccessory: EnumAvatarAssetType
	ShirtAccessory: EnumAvatarAssetType
	PantsAccessory: EnumAvatarAssetType
	JacketAccessory: EnumAvatarAssetType
	SweaterAccessory: EnumAvatarAssetType
	ShortsAccessory: EnumAvatarAssetType
	LeftShoeAccessory: EnumAvatarAssetType
	RightShoeAccessory: EnumAvatarAssetType
	DressSkirtAccessory: EnumAvatarAssetType
	EyebrowAccessory: EnumAvatarAssetType
	EyelashAccessory: EnumAvatarAssetType
	DynamicHead: EnumAvatarAssetType
end
declare class EnumAvatarChatServiceFeature extends EnumItem end
declare class EnumAvatarChatServiceFeature_INTERNAL extends Enum
	None: EnumAvatarChatServiceFeature
	UniverseAudio: EnumAvatarChatServiceFeature
	UniverseVideo: EnumAvatarChatServiceFeature
	PlaceAudio: EnumAvatarChatServiceFeature
	PlaceVideo: EnumAvatarChatServiceFeature
	UserAudioEligible: EnumAvatarChatServiceFeature
	UserAudio: EnumAvatarChatServiceFeature
	UserVideoEligible: EnumAvatarChatServiceFeature
	UserVideo: EnumAvatarChatServiceFeature
	UserBanned: EnumAvatarChatServiceFeature
end
declare class EnumAvatarContextMenuOption extends EnumItem end
declare class EnumAvatarContextMenuOption_INTERNAL extends Enum
	Friend: EnumAvatarContextMenuOption
	Chat: EnumAvatarContextMenuOption
	Emote: EnumAvatarContextMenuOption
	InspectMenu: EnumAvatarContextMenuOption
end
declare class EnumAvatarItemType extends EnumItem end
declare class EnumAvatarItemType_INTERNAL extends Enum
	Asset: EnumAvatarItemType
	Bundle: EnumAvatarItemType
end
declare class EnumAvatarJointUpgrade extends EnumItem end
declare class EnumAvatarJointUpgrade_INTERNAL extends Enum
	Default: EnumAvatarJointUpgrade
	Enabled: EnumAvatarJointUpgrade
	Disabled: EnumAvatarJointUpgrade
end
declare class EnumAvatarPromptResult extends EnumItem end
declare class EnumAvatarPromptResult_INTERNAL extends Enum
	Success: EnumAvatarPromptResult
	PermissionDenied: EnumAvatarPromptResult
	Failed: EnumAvatarPromptResult
end
declare class EnumAvatarThumbnailCustomizationType extends EnumItem end
declare class EnumAvatarThumbnailCustomizationType_INTERNAL extends Enum
	Closeup: EnumAvatarThumbnailCustomizationType
	FullBody: EnumAvatarThumbnailCustomizationType
end
declare class EnumAvatarUnificationMode extends EnumItem end
declare class EnumAvatarUnificationMode_INTERNAL extends Enum
	Default: EnumAvatarUnificationMode
	Disabled: EnumAvatarUnificationMode
	Enabled: EnumAvatarUnificationMode
end
declare class EnumAxis extends EnumItem end
declare class EnumAxis_INTERNAL extends Enum
	X: EnumAxis
	Y: EnumAxis
	Z: EnumAxis
end
declare class EnumBinType extends EnumItem end
declare class EnumBinType_INTERNAL extends Enum
	Script: EnumBinType
	GameTool: EnumBinType
	Grab: EnumBinType
	Clone: EnumBinType
	Hammer: EnumBinType
end
declare class EnumBodyPart extends EnumItem end
declare class EnumBodyPart_INTERNAL extends Enum
	Head: EnumBodyPart
	Torso: EnumBodyPart
	LeftArm: EnumBodyPart
	RightArm: EnumBodyPart
	LeftLeg: EnumBodyPart
	RightLeg: EnumBodyPart
end
declare class EnumBodyPartR15 extends EnumItem end
declare class EnumBodyPartR15_INTERNAL extends Enum
	Head: EnumBodyPartR15
	UpperTorso: EnumBodyPartR15
	LowerTorso: EnumBodyPartR15
	LeftFoot: EnumBodyPartR15
	LeftLowerLeg: EnumBodyPartR15
	LeftUpperLeg: EnumBodyPartR15
	RightFoot: EnumBodyPartR15
	RightLowerLeg: EnumBodyPartR15
	RightUpperLeg: EnumBodyPartR15
	LeftHand: EnumBodyPartR15
	LeftLowerArm: EnumBodyPartR15
	LeftUpperArm: EnumBodyPartR15
	RightHand: EnumBodyPartR15
	RightLowerArm: EnumBodyPartR15
	RightUpperArm: EnumBodyPartR15
	RootPart: EnumBodyPartR15
	Unknown: EnumBodyPartR15
end
declare class EnumBorderMode extends EnumItem end
declare class EnumBorderMode_INTERNAL extends Enum
	Outline: EnumBorderMode
	Middle: EnumBorderMode
	Inset: EnumBorderMode
end
declare class EnumBreakReason extends EnumItem end
declare class EnumBreakReason_INTERNAL extends Enum
	Other: EnumBreakReason
	Error: EnumBreakReason
	UserBreakpoint: EnumBreakReason
	SpecialBreakpoint: EnumBreakReason
end
declare class EnumBreakpointRemoveReason extends EnumItem end
declare class EnumBreakpointRemoveReason_INTERNAL extends Enum
	Requested: EnumBreakpointRemoveReason
	ScriptChanged: EnumBreakpointRemoveReason
	ScriptRemoved: EnumBreakpointRemoveReason
end
declare class EnumBulkMoveMode extends EnumItem end
declare class EnumBulkMoveMode_INTERNAL extends Enum
	FireAllEvents: EnumBulkMoveMode
	FireCFrameChanged: EnumBulkMoveMode
end
declare class EnumBundleType extends EnumItem end
declare class EnumBundleType_INTERNAL extends Enum
	BodyParts: EnumBundleType
	Animations: EnumBundleType
	Shoes: EnumBundleType
	DynamicHead: EnumBundleType
	DynamicHeadAvatar: EnumBundleType
end
declare class EnumButton extends EnumItem end
declare class EnumButton_INTERNAL extends Enum
	Jump: EnumButton
	Dismount: EnumButton
end
declare class EnumButtonStyle extends EnumItem end
declare class EnumButtonStyle_INTERNAL extends Enum
	Custom: EnumButtonStyle
	RobloxButtonDefault: EnumButtonStyle
	RobloxButton: EnumButtonStyle
	RobloxRoundButton: EnumButtonStyle
	RobloxRoundDefaultButton: EnumButtonStyle
	RobloxRoundDropdownButton: EnumButtonStyle
end
declare class EnumCageType extends EnumItem end
declare class EnumCageType_INTERNAL extends Enum
	Inner: EnumCageType
	Outer: EnumCageType
end
declare class EnumCameraMode extends EnumItem end
declare class EnumCameraMode_INTERNAL extends Enum
	Classic: EnumCameraMode
	LockFirstPerson: EnumCameraMode
end
declare class EnumCameraPanMode extends EnumItem end
declare class EnumCameraPanMode_INTERNAL extends Enum
	Classic: EnumCameraPanMode
	EdgeBump: EnumCameraPanMode
end
declare class EnumCameraType extends EnumItem end
declare class EnumCameraType_INTERNAL extends Enum
	Fixed: EnumCameraType
	Watch: EnumCameraType
	Attach: EnumCameraType
	Track: EnumCameraType
	Follow: EnumCameraType
	Custom: EnumCameraType
	Scriptable: EnumCameraType
	Orbital: EnumCameraType
end
declare class EnumCatalogCategoryFilter extends EnumItem end
declare class EnumCatalogCategoryFilter_INTERNAL extends Enum
	None: EnumCatalogCategoryFilter
	Featured: EnumCatalogCategoryFilter
	Collectibles: EnumCatalogCategoryFilter
	CommunityCreations: EnumCatalogCategoryFilter
	Premium: EnumCatalogCategoryFilter
	Recommended: EnumCatalogCategoryFilter
end
declare class EnumCatalogSortAggregation extends EnumItem end
declare class EnumCatalogSortAggregation_INTERNAL extends Enum
	Past12Hours: EnumCatalogSortAggregation
	PastDay: EnumCatalogSortAggregation
	Past3Days: EnumCatalogSortAggregation
	PastWeek: EnumCatalogSortAggregation
	PastMonth: EnumCatalogSortAggregation
	AllTime: EnumCatalogSortAggregation
end
declare class EnumCatalogSortType extends EnumItem end
declare class EnumCatalogSortType_INTERNAL extends Enum
	Relevance: EnumCatalogSortType
	PriceHighToLow: EnumCatalogSortType
	PriceLowToHigh: EnumCatalogSortType
	MostFavorited: EnumCatalogSortType
	RecentlyCreated: EnumCatalogSortType
	Bestselling: EnumCatalogSortType
end
declare class EnumCellBlock extends EnumItem end
declare class EnumCellBlock_INTERNAL extends Enum
	Solid: EnumCellBlock
	VerticalWedge: EnumCellBlock
	CornerWedge: EnumCellBlock
	InverseCornerWedge: EnumCellBlock
	HorizontalWedge: EnumCellBlock
end
declare class EnumCellMaterial extends EnumItem end
declare class EnumCellMaterial_INTERNAL extends Enum
	Empty: EnumCellMaterial
	Grass: EnumCellMaterial
	Sand: EnumCellMaterial
	Brick: EnumCellMaterial
	Granite: EnumCellMaterial
	Asphalt: EnumCellMaterial
	Iron: EnumCellMaterial
	Aluminum: EnumCellMaterial
	Gold: EnumCellMaterial
	WoodPlank: EnumCellMaterial
	WoodLog: EnumCellMaterial
	Gravel: EnumCellMaterial
	CinderBlock: EnumCellMaterial
	MossyStone: EnumCellMaterial
	Cement: EnumCellMaterial
	RedPlastic: EnumCellMaterial
	BluePlastic: EnumCellMaterial
	Water: EnumCellMaterial
end
declare class EnumCellOrientation extends EnumItem end
declare class EnumCellOrientation_INTERNAL extends Enum
	NegZ: EnumCellOrientation
	X: EnumCellOrientation
	Z: EnumCellOrientation
	NegX: EnumCellOrientation
end
declare class EnumCenterDialogType extends EnumItem end
declare class EnumCenterDialogType_INTERNAL extends Enum
	UnsolicitedDialog: EnumCenterDialogType
	PlayerInitiatedDialog: EnumCenterDialogType
	ModalDialog: EnumCenterDialogType
	QuitDialog: EnumCenterDialogType
end
declare class EnumChatCallbackType extends EnumItem end
declare class EnumChatCallbackType_INTERNAL extends Enum
	OnCreatingChatWindow: EnumChatCallbackType
	OnClientSendingMessage: EnumChatCallbackType
	OnClientFormattingMessage: EnumChatCallbackType
	OnServerReceivingMessage: EnumChatCallbackType
end
declare class EnumChatColor extends EnumItem end
declare class EnumChatColor_INTERNAL extends Enum
	Blue: EnumChatColor
	Green: EnumChatColor
	Red: EnumChatColor
	White: EnumChatColor
end
declare class EnumChatMode extends EnumItem end
declare class EnumChatMode_INTERNAL extends Enum
	Menu: EnumChatMode
	TextAndMenu: EnumChatMode
end
declare class EnumChatPrivacyMode extends EnumItem end
declare class EnumChatPrivacyMode_INTERNAL extends Enum
	AllUsers: EnumChatPrivacyMode
	NoOne: EnumChatPrivacyMode
	Friends: EnumChatPrivacyMode
end
declare class EnumChatStyle extends EnumItem end
declare class EnumChatStyle_INTERNAL extends Enum
	Classic: EnumChatStyle
	Bubble: EnumChatStyle
	ClassicAndBubble: EnumChatStyle
end
declare class EnumChatVersion extends EnumItem end
declare class EnumChatVersion_INTERNAL extends Enum
	LegacyChatService: EnumChatVersion
	TextChatService: EnumChatVersion
end
declare class EnumClientAnimatorThrottlingMode extends EnumItem end
declare class EnumClientAnimatorThrottlingMode_INTERNAL extends Enum
	Default: EnumClientAnimatorThrottlingMode
	Disabled: EnumClientAnimatorThrottlingMode
	Enabled: EnumClientAnimatorThrottlingMode
end
declare class EnumCollisionFidelity extends EnumItem end
declare class EnumCollisionFidelity_INTERNAL extends Enum
	Default: EnumCollisionFidelity
	Hull: EnumCollisionFidelity
	Box: EnumCollisionFidelity
	PreciseConvexDecomposition: EnumCollisionFidelity
	DynamicPreciseConvexDecomposition: EnumCollisionFidelity
end
declare class EnumCommandPermission extends EnumItem end
declare class EnumCommandPermission_INTERNAL extends Enum
	Plugin: EnumCommandPermission
	LocalUser: EnumCommandPermission
end
declare class EnumCompileTarget extends EnumItem end
declare class EnumCompileTarget_INTERNAL extends Enum
	Client: EnumCompileTarget
	CoreScript: EnumCompileTarget
	Studio: EnumCompileTarget
	CoreScriptRaw: EnumCompileTarget
end
declare class EnumCompletionItemKind extends EnumItem end
declare class EnumCompletionItemKind_INTERNAL extends Enum
	Text: EnumCompletionItemKind
	Method: EnumCompletionItemKind
	Function: EnumCompletionItemKind
	Constructor: EnumCompletionItemKind
	Field: EnumCompletionItemKind
	Variable: EnumCompletionItemKind
	Class: EnumCompletionItemKind
	Interface: EnumCompletionItemKind
	Module: EnumCompletionItemKind
	Property: EnumCompletionItemKind
	Unit: EnumCompletionItemKind
	Value: EnumCompletionItemKind
	Enum: EnumCompletionItemKind
	Keyword: EnumCompletionItemKind
	Snippet: EnumCompletionItemKind
	Color: EnumCompletionItemKind
	File: EnumCompletionItemKind
	Reference: EnumCompletionItemKind
	Folder: EnumCompletionItemKind
	EnumMember: EnumCompletionItemKind
	Constant: EnumCompletionItemKind
	Struct: EnumCompletionItemKind
	Event: EnumCompletionItemKind
	Operator: EnumCompletionItemKind
	TypeParameter: EnumCompletionItemKind
end
declare class EnumCompletionItemTag extends EnumItem end
declare class EnumCompletionItemTag_INTERNAL extends Enum
	Deprecated: EnumCompletionItemTag
	IncorrectIndexType: EnumCompletionItemTag
	PluginPermissions: EnumCompletionItemTag
	CommandLinePermissions: EnumCompletionItemTag
	RobloxPermissions: EnumCompletionItemTag
	AddParens: EnumCompletionItemTag
	PutCursorInParens: EnumCompletionItemTag
	TypeCorrect: EnumCompletionItemTag
	ClientServerBoundaryViolation: EnumCompletionItemTag
end
declare class EnumCompletionTriggerKind extends EnumItem end
declare class EnumCompletionTriggerKind_INTERNAL extends Enum
	Invoked: EnumCompletionTriggerKind
	TriggerCharacter: EnumCompletionTriggerKind
	TriggerForIncompleteCompletions: EnumCompletionTriggerKind
end
declare class EnumComputerCameraMovementMode extends EnumItem end
declare class EnumComputerCameraMovementMode_INTERNAL extends Enum
	Default: EnumComputerCameraMovementMode
	Follow: EnumComputerCameraMovementMode
	Classic: EnumComputerCameraMovementMode
	Orbital: EnumComputerCameraMovementMode
	CameraToggle: EnumComputerCameraMovementMode
end
declare class EnumComputerMovementMode extends EnumItem end
declare class EnumComputerMovementMode_INTERNAL extends Enum
	Default: EnumComputerMovementMode
	KeyboardMouse: EnumComputerMovementMode
	ClickToMove: EnumComputerMovementMode
end
declare class EnumConnectionError extends EnumItem end
declare class EnumConnectionError_INTERNAL extends Enum
	OK: EnumConnectionError
	Unknown: EnumConnectionError
	DisconnectErrors: EnumConnectionError
	DisconnectBadhash: EnumConnectionError
	DisconnectSecurityKeyMismatch: EnumConnectionError
	DisconnectNewSecurityKeyMismatch: EnumConnectionError
	DisconnectProtocolMismatch: EnumConnectionError
	DisconnectReceivePacketError: EnumConnectionError
	DisconnectReceivePacketStreamError: EnumConnectionError
	DisconnectSendPacketError: EnumConnectionError
	DisconnectIllegalTeleport: EnumConnectionError
	DisconnectDuplicatePlayer: EnumConnectionError
	DisconnectDuplicateTicket: EnumConnectionError
	DisconnectTimeout: EnumConnectionError
	DisconnectLuaKick: EnumConnectionError
	DisconnectOnRemoteSysStats: EnumConnectionError
	DisconnectHashTimeout: EnumConnectionError
	DisconnectCloudEditKick: EnumConnectionError
	DisconnectPlayerless: EnumConnectionError
	DisconnectEvicted: EnumConnectionError
	DisconnectDevMaintenance: EnumConnectionError
	DisconnectRobloxMaintenance: EnumConnectionError
	DisconnectRejoin: EnumConnectionError
	DisconnectConnectionLost: EnumConnectionError
	DisconnectIdle: EnumConnectionError
	DisconnectRaknetErrors: EnumConnectionError
	DisconnectWrongVersion: EnumConnectionError
	DisconnectBySecurityPolicy: EnumConnectionError
	DisconnectBlockedIP: EnumConnectionError
	DisconnectClientFailure: EnumConnectionError
	DisconnectClientRequest: EnumConnectionError
	DisconnectPrivateServerKickout: EnumConnectionError
	DisconnectModeratedGame: EnumConnectionError
	DisconnectRomarkEndOfTest: EnumConnectionError
	ReplicatorTimeout: EnumConnectionError
	PlayerRemoved: EnumConnectionError
	DisconnectOutOfMemoryKeepPlayingLeave: EnumConnectionError
	DisconnectCollaboratorPermissionRevoked: EnumConnectionError
	DisconnectCollaboratorUnderage: EnumConnectionError
	PlacelaunchErrors: EnumConnectionError
	PlacelaunchDisabled: EnumConnectionError
	PlacelaunchError: EnumConnectionError
	PlacelaunchGameEnded: EnumConnectionError
	PlacelaunchGameFull: EnumConnectionError
	PlacelaunchUserLeft: EnumConnectionError
	PlacelaunchRestricted: EnumConnectionError
	PlacelaunchUnauthorized: EnumConnectionError
	PlacelaunchFlooded: EnumConnectionError
	PlacelaunchHashExpired: EnumConnectionError
	PlacelaunchHashException: EnumConnectionError
	PlacelaunchPartyCannotFit: EnumConnectionError
	PlacelaunchHttpError: EnumConnectionError
	PlacelaunchUserPrivacyUnauthorized: EnumConnectionError
	PlacelaunchCustomMessage: EnumConnectionError
	PlacelaunchOtherError: EnumConnectionError
	TeleportErrors: EnumConnectionError
	TeleportFailure: EnumConnectionError
	TeleportGameNotFound: EnumConnectionError
	TeleportGameEnded: EnumConnectionError
	TeleportGameFull: EnumConnectionError
	TeleportUnauthorized: EnumConnectionError
	TeleportFlooded: EnumConnectionError
	TeleportIsTeleporting: EnumConnectionError
end
declare class EnumConnectionState extends EnumItem end
declare class EnumConnectionState_INTERNAL extends Enum
	Connected: EnumConnectionState
	Disconnected: EnumConnectionState
end
declare class EnumContextActionPriority extends EnumItem end
declare class EnumContextActionPriority_INTERNAL extends Enum
	Low: EnumContextActionPriority
	Medium: EnumContextActionPriority
	High: EnumContextActionPriority
end
declare class EnumContextActionResult extends EnumItem end
declare class EnumContextActionResult_INTERNAL extends Enum
	Pass: EnumContextActionResult
	Sink: EnumContextActionResult
end
declare class EnumControlMode extends EnumItem end
declare class EnumControlMode_INTERNAL extends Enum
	MouseLockSwitch: EnumControlMode
	Classic: EnumControlMode
end
declare class EnumCoreGuiType extends EnumItem end
declare class EnumCoreGuiType_INTERNAL extends Enum
	PlayerList: EnumCoreGuiType
	Health: EnumCoreGuiType
	Backpack: EnumCoreGuiType
	Chat: EnumCoreGuiType
	All: EnumCoreGuiType
	EmotesMenu: EnumCoreGuiType
	SelfView: EnumCoreGuiType
end
declare class EnumCreateOutfitFailure extends EnumItem end
declare class EnumCreateOutfitFailure_INTERNAL extends Enum
	InvalidName: EnumCreateOutfitFailure
	OutfitLimitReached: EnumCreateOutfitFailure
	Other: EnumCreateOutfitFailure
end
declare class EnumCreatorType extends EnumItem end
declare class EnumCreatorType_INTERNAL extends Enum
	User: EnumCreatorType
	Group: EnumCreatorType
end
declare class EnumCreatorTypeFilter extends EnumItem end
declare class EnumCreatorTypeFilter_INTERNAL extends Enum
	User: EnumCreatorTypeFilter
	Group: EnumCreatorTypeFilter
	All: EnumCreatorTypeFilter
end
declare class EnumCurrencyType extends EnumItem end
declare class EnumCurrencyType_INTERNAL extends Enum
	Default: EnumCurrencyType
	Robux: EnumCurrencyType
	Tix: EnumCurrencyType
end
declare class EnumCustomCameraMode extends EnumItem end
declare class EnumCustomCameraMode_INTERNAL extends Enum
	Default: EnumCustomCameraMode
	Follow: EnumCustomCameraMode
	Classic: EnumCustomCameraMode
end
declare class EnumDataStoreRequestType extends EnumItem end
declare class EnumDataStoreRequestType_INTERNAL extends Enum
	GetAsync: EnumDataStoreRequestType
	SetIncrementAsync: EnumDataStoreRequestType
	UpdateAsync: EnumDataStoreRequestType
	GetSortedAsync: EnumDataStoreRequestType
	SetIncrementSortedAsync: EnumDataStoreRequestType
	OnUpdate: EnumDataStoreRequestType
end
declare class EnumDeathStyle extends EnumItem end
declare class EnumDeathStyle_INTERNAL extends Enum
	Default: EnumDeathStyle
	ClassicBreakApart: EnumDeathStyle
	NonGraphic: EnumDeathStyle
	Scriptable: EnumDeathStyle
end
declare class EnumDebuggerEndReason extends EnumItem end
declare class EnumDebuggerEndReason_INTERNAL extends Enum
	ClientRequest: EnumDebuggerEndReason
	Timeout: EnumDebuggerEndReason
	InvalidHost: EnumDebuggerEndReason
	Disconnected: EnumDebuggerEndReason
	ServerShutdown: EnumDebuggerEndReason
	ServerProtocolMismatch: EnumDebuggerEndReason
	ConfigurationFailed: EnumDebuggerEndReason
	RpcError: EnumDebuggerEndReason
end
declare class EnumDebuggerExceptionBreakMode extends EnumItem end
declare class EnumDebuggerExceptionBreakMode_INTERNAL extends Enum
	Never: EnumDebuggerExceptionBreakMode
	Unhandled: EnumDebuggerExceptionBreakMode
	Always: EnumDebuggerExceptionBreakMode
end
declare class EnumDebuggerFrameType extends EnumItem end
declare class EnumDebuggerFrameType_INTERNAL extends Enum
	C: EnumDebuggerFrameType
	Lua: EnumDebuggerFrameType
end
declare class EnumDebuggerPauseReason extends EnumItem end
declare class EnumDebuggerPauseReason_INTERNAL extends Enum
	Unknown: EnumDebuggerPauseReason
	Requested: EnumDebuggerPauseReason
	Breakpoint: EnumDebuggerPauseReason
	Exception: EnumDebuggerPauseReason
	SingleStep: EnumDebuggerPauseReason
	Entrypoint: EnumDebuggerPauseReason
end
declare class EnumDebuggerStatus extends EnumItem end
declare class EnumDebuggerStatus_INTERNAL extends Enum
	Success: EnumDebuggerStatus
	Timeout: EnumDebuggerStatus
	ConnectionLost: EnumDebuggerStatus
	InvalidResponse: EnumDebuggerStatus
	InternalError: EnumDebuggerStatus
	InvalidState: EnumDebuggerStatus
	RpcError: EnumDebuggerStatus
	InvalidArgument: EnumDebuggerStatus
	ConnectionClosed: EnumDebuggerStatus
end
declare class EnumDevCameraOcclusionMode extends EnumItem end
declare class EnumDevCameraOcclusionMode_INTERNAL extends Enum
	Zoom: EnumDevCameraOcclusionMode
	Invisicam: EnumDevCameraOcclusionMode
end
declare class EnumDevComputerCameraMovementMode extends EnumItem end
declare class EnumDevComputerCameraMovementMode_INTERNAL extends Enum
	UserChoice: EnumDevComputerCameraMovementMode
	Classic: EnumDevComputerCameraMovementMode
	Follow: EnumDevComputerCameraMovementMode
	Orbital: EnumDevComputerCameraMovementMode
	CameraToggle: EnumDevComputerCameraMovementMode
end
declare class EnumDevComputerMovementMode extends EnumItem end
declare class EnumDevComputerMovementMode_INTERNAL extends Enum
	UserChoice: EnumDevComputerMovementMode
	KeyboardMouse: EnumDevComputerMovementMode
	ClickToMove: EnumDevComputerMovementMode
	Scriptable: EnumDevComputerMovementMode
end
declare class EnumDevTouchCameraMovementMode extends EnumItem end
declare class EnumDevTouchCameraMovementMode_INTERNAL extends Enum
	UserChoice: EnumDevTouchCameraMovementMode
	Classic: EnumDevTouchCameraMovementMode
	Follow: EnumDevTouchCameraMovementMode
	Orbital: EnumDevTouchCameraMovementMode
end
declare class EnumDevTouchMovementMode extends EnumItem end
declare class EnumDevTouchMovementMode_INTERNAL extends Enum
	UserChoice: EnumDevTouchMovementMode
	Thumbstick: EnumDevTouchMovementMode
	DPad: EnumDevTouchMovementMode
	Thumbpad: EnumDevTouchMovementMode
	ClickToMove: EnumDevTouchMovementMode
	Scriptable: EnumDevTouchMovementMode
	DynamicThumbstick: EnumDevTouchMovementMode
end
declare class EnumDeveloperMemoryTag extends EnumItem end
declare class EnumDeveloperMemoryTag_INTERNAL extends Enum
	Internal: EnumDeveloperMemoryTag
	HttpCache: EnumDeveloperMemoryTag
	Instances: EnumDeveloperMemoryTag
	Signals: EnumDeveloperMemoryTag
	LuaHeap: EnumDeveloperMemoryTag
	Script: EnumDeveloperMemoryTag
	PhysicsCollision: EnumDeveloperMemoryTag
	PhysicsParts: EnumDeveloperMemoryTag
	GraphicsSolidModels: EnumDeveloperMemoryTag
	GraphicsMeshParts: EnumDeveloperMemoryTag
	GraphicsParticles: EnumDeveloperMemoryTag
	GraphicsParts: EnumDeveloperMemoryTag
	GraphicsSpatialHash: EnumDeveloperMemoryTag
	GraphicsTerrain: EnumDeveloperMemoryTag
	GraphicsTexture: EnumDeveloperMemoryTag
	GraphicsTextureCharacter: EnumDeveloperMemoryTag
	Sounds: EnumDeveloperMemoryTag
	StreamingSounds: EnumDeveloperMemoryTag
	TerrainVoxels: EnumDeveloperMemoryTag
	Gui: EnumDeveloperMemoryTag
	Animation: EnumDeveloperMemoryTag
	Navigation: EnumDeveloperMemoryTag
	GeometryCSG: EnumDeveloperMemoryTag
end
declare class EnumDeviceType extends EnumItem end
declare class EnumDeviceType_INTERNAL extends Enum
	Unknown: EnumDeviceType
	Desktop: EnumDeviceType
	Tablet: EnumDeviceType
	Phone: EnumDeviceType
end
declare class EnumDialogBehaviorType extends EnumItem end
declare class EnumDialogBehaviorType_INTERNAL extends Enum
	SinglePlayer: EnumDialogBehaviorType
	MultiplePlayers: EnumDialogBehaviorType
end
declare class EnumDialogPurpose extends EnumItem end
declare class EnumDialogPurpose_INTERNAL extends Enum
	Quest: EnumDialogPurpose
	Help: EnumDialogPurpose
	Shop: EnumDialogPurpose
end
declare class EnumDialogTone extends EnumItem end
declare class EnumDialogTone_INTERNAL extends Enum
	Neutral: EnumDialogTone
	Friendly: EnumDialogTone
	Enemy: EnumDialogTone
end
declare class EnumDominantAxis extends EnumItem end
declare class EnumDominantAxis_INTERNAL extends Enum
	Width: EnumDominantAxis
	Height: EnumDominantAxis
end
declare class EnumDraftStatusCode extends EnumItem end
declare class EnumDraftStatusCode_INTERNAL extends Enum
	OK: EnumDraftStatusCode
	DraftOutdated: EnumDraftStatusCode
	ScriptRemoved: EnumDraftStatusCode
	DraftCommitted: EnumDraftStatusCode
end
declare class EnumDragDetectorDragStyle extends EnumItem end
declare class EnumDragDetectorDragStyle_INTERNAL extends Enum
	TranslateLine: EnumDragDetectorDragStyle
	TranslatePlane: EnumDragDetectorDragStyle
	TranslatePlaneOrLine: EnumDragDetectorDragStyle
	TranslateLineOrPlane: EnumDragDetectorDragStyle
	TranslateViewPlane: EnumDragDetectorDragStyle
	RotateAxis: EnumDragDetectorDragStyle
	RotateTrackball: EnumDragDetectorDragStyle
	Scriptable: EnumDragDetectorDragStyle
	BestForDevice: EnumDragDetectorDragStyle
end
declare class EnumDragDetectorResponseStyle extends EnumItem end
declare class EnumDragDetectorResponseStyle_INTERNAL extends Enum
	Geometric: EnumDragDetectorResponseStyle
	Physical: EnumDragDetectorResponseStyle
	Custom: EnumDragDetectorResponseStyle
end
declare class EnumDraggerCoordinateSpace extends EnumItem end
declare class EnumDraggerCoordinateSpace_INTERNAL extends Enum
	Object: EnumDraggerCoordinateSpace
	World: EnumDraggerCoordinateSpace
end
declare class EnumDraggerMovementMode extends EnumItem end
declare class EnumDraggerMovementMode_INTERNAL extends Enum
	Geometric: EnumDraggerMovementMode
	Physical: EnumDraggerMovementMode
end
declare class EnumEasingDirection extends EnumItem end
declare class EnumEasingDirection_INTERNAL extends Enum
	In: EnumEasingDirection
	Out: EnumEasingDirection
	InOut: EnumEasingDirection
end
declare class EnumEasingStyle extends EnumItem end
declare class EnumEasingStyle_INTERNAL extends Enum
	Linear: EnumEasingStyle
	Sine: EnumEasingStyle
	Back: EnumEasingStyle
	Quad: EnumEasingStyle
	Quart: EnumEasingStyle
	Quint: EnumEasingStyle
	Bounce: EnumEasingStyle
	Elastic: EnumEasingStyle
	Exponential: EnumEasingStyle
	Circular: EnumEasingStyle
	Cubic: EnumEasingStyle
end
declare class EnumElasticBehavior extends EnumItem end
declare class EnumElasticBehavior_INTERNAL extends Enum
	WhenScrollable: EnumElasticBehavior
	Always: EnumElasticBehavior
	Never: EnumElasticBehavior
end
declare class EnumEnviromentalPhysicsThrottle extends EnumItem end
declare class EnumEnviromentalPhysicsThrottle_INTERNAL extends Enum
	DefaultAuto: EnumEnviromentalPhysicsThrottle
	Disabled: EnumEnviromentalPhysicsThrottle
	Always: EnumEnviromentalPhysicsThrottle
	Skip2: EnumEnviromentalPhysicsThrottle
	Skip4: EnumEnviromentalPhysicsThrottle
	Skip8: EnumEnviromentalPhysicsThrottle
	Skip16: EnumEnviromentalPhysicsThrottle
end
declare class EnumExperienceAuthScope extends EnumItem end
declare class EnumExperienceAuthScope_INTERNAL extends Enum
	DefaultScope: EnumExperienceAuthScope
	CreatorAssetsCreate: EnumExperienceAuthScope
end
declare class EnumExplosionType extends EnumItem end
declare class EnumExplosionType_INTERNAL extends Enum
	NoCraters: EnumExplosionType
	Craters: EnumExplosionType
end
declare class EnumFacialAnimationStreamingState extends EnumItem end
declare class EnumFacialAnimationStreamingState_INTERNAL extends Enum
	None: EnumFacialAnimationStreamingState
	Audio: EnumFacialAnimationStreamingState
	Video: EnumFacialAnimationStreamingState
	Place: EnumFacialAnimationStreamingState
	Server: EnumFacialAnimationStreamingState
end
declare class EnumFieldOfViewMode extends EnumItem end
declare class EnumFieldOfViewMode_INTERNAL extends Enum
	Vertical: EnumFieldOfViewMode
	Diagonal: EnumFieldOfViewMode
	MaxAxis: EnumFieldOfViewMode
end
declare class EnumFillDirection extends EnumItem end
declare class EnumFillDirection_INTERNAL extends Enum
	Horizontal: EnumFillDirection
	Vertical: EnumFillDirection
end
declare class EnumFilterResult extends EnumItem end
declare class EnumFilterResult_INTERNAL extends Enum
	Rejected: EnumFilterResult
	Accepted: EnumFilterResult
end
declare class EnumFinishRecordingOperation extends EnumItem end
declare class EnumFinishRecordingOperation_INTERNAL extends Enum
	Cancel: EnumFinishRecordingOperation
	Commit: EnumFinishRecordingOperation
	Append: EnumFinishRecordingOperation
end
declare class EnumFluidForces extends EnumItem end
declare class EnumFluidForces_INTERNAL extends Enum
	Default: EnumFluidForces
	Experimental: EnumFluidForces
end
declare class EnumFont extends EnumItem end
declare class EnumFont_INTERNAL extends Enum
	Legacy: EnumFont
	Arial: EnumFont
	ArialBold: EnumFont
	SourceSans: EnumFont
	SourceSansBold: EnumFont
	SourceSansSemibold: EnumFont
	SourceSansLight: EnumFont
	SourceSansItalic: EnumFont
	Bodoni: EnumFont
	Garamond: EnumFont
	Cartoon: EnumFont
	Code: EnumFont
	Highway: EnumFont
	SciFi: EnumFont
	Arcade: EnumFont
	Fantasy: EnumFont
	Antique: EnumFont
	Gotham: EnumFont
	GothamMedium: EnumFont
	GothamBold: EnumFont
	GothamBlack: EnumFont
	AmaticSC: EnumFont
	Bangers: EnumFont
	Creepster: EnumFont
	DenkOne: EnumFont
	Fondamento: EnumFont
	FredokaOne: EnumFont
	GrenzeGotisch: EnumFont
	IndieFlower: EnumFont
	JosefinSans: EnumFont
	Jura: EnumFont
	Kalam: EnumFont
	LuckiestGuy: EnumFont
	Merriweather: EnumFont
	Michroma: EnumFont
	Nunito: EnumFont
	Oswald: EnumFont
	PatrickHand: EnumFont
	PermanentMarker: EnumFont
	Roboto: EnumFont
	RobotoCondensed: EnumFont
	RobotoMono: EnumFont
	Sarpanch: EnumFont
	SpecialElite: EnumFont
	TitilliumWeb: EnumFont
	Ubuntu: EnumFont
	Unknown: EnumFont
end
declare class EnumFontSize extends EnumItem end
declare class EnumFontSize_INTERNAL extends Enum
	Size8: EnumFontSize
	Size9: EnumFontSize
	Size10: EnumFontSize
	Size11: EnumFontSize
	Size12: EnumFontSize
	Size14: EnumFontSize
	Size18: EnumFontSize
	Size24: EnumFontSize
	Size36: EnumFontSize
	Size48: EnumFontSize
	Size28: EnumFontSize
	Size32: EnumFontSize
	Size42: EnumFontSize
	Size60: EnumFontSize
	Size96: EnumFontSize
end
declare class EnumFontStyle extends EnumItem end
declare class EnumFontStyle_INTERNAL extends Enum
	Normal: EnumFontStyle
	Italic: EnumFontStyle
end
declare class EnumFontWeight extends EnumItem end
declare class EnumFontWeight_INTERNAL extends Enum
	Thin: EnumFontWeight
	ExtraLight: EnumFontWeight
	Light: EnumFontWeight
	Regular: EnumFontWeight
	Medium: EnumFontWeight
	SemiBold: EnumFontWeight
	Bold: EnumFontWeight
	ExtraBold: EnumFontWeight
	Heavy: EnumFontWeight
end
declare class EnumForceLimitMode extends EnumItem end
declare class EnumForceLimitMode_INTERNAL extends Enum
	Magnitude: EnumForceLimitMode
	PerAxis: EnumForceLimitMode
end
declare class EnumFormFactor extends EnumItem end
declare class EnumFormFactor_INTERNAL extends Enum
	Symmetric: EnumFormFactor
	Brick: EnumFormFactor
	Plate: EnumFormFactor
	Custom: EnumFormFactor
end
declare class EnumFrameStyle extends EnumItem end
declare class EnumFrameStyle_INTERNAL extends Enum
	Custom: EnumFrameStyle
	ChatBlue: EnumFrameStyle
	RobloxSquare: EnumFrameStyle
	RobloxRound: EnumFrameStyle
	ChatGreen: EnumFrameStyle
	ChatRed: EnumFrameStyle
	DropShadow: EnumFrameStyle
end
declare class EnumFramerateManagerMode extends EnumItem end
declare class EnumFramerateManagerMode_INTERNAL extends Enum
	Automatic: EnumFramerateManagerMode
	On: EnumFramerateManagerMode
	Off: EnumFramerateManagerMode
end
declare class EnumFriendRequestEvent extends EnumItem end
declare class EnumFriendRequestEvent_INTERNAL extends Enum
	Issue: EnumFriendRequestEvent
	Revoke: EnumFriendRequestEvent
	Accept: EnumFriendRequestEvent
	Deny: EnumFriendRequestEvent
end
declare class EnumFriendStatus extends EnumItem end
declare class EnumFriendStatus_INTERNAL extends Enum
	Unknown: EnumFriendStatus
	NotFriend: EnumFriendStatus
	Friend: EnumFriendStatus
	FriendRequestSent: EnumFriendStatus
	FriendRequestReceived: EnumFriendStatus
end
declare class EnumFunctionalTestResult extends EnumItem end
declare class EnumFunctionalTestResult_INTERNAL extends Enum
	Passed: EnumFunctionalTestResult
	Warning: EnumFunctionalTestResult
	Error: EnumFunctionalTestResult
end
declare class EnumGameAvatarType extends EnumItem end
declare class EnumGameAvatarType_INTERNAL extends Enum
	R6: EnumGameAvatarType
	R15: EnumGameAvatarType
	PlayerChoice: EnumGameAvatarType
end
declare class EnumGearGenreSetting extends EnumItem end
declare class EnumGearGenreSetting_INTERNAL extends Enum
	AllGenres: EnumGearGenreSetting
	MatchingGenreOnly: EnumGearGenreSetting
end
declare class EnumGearType extends EnumItem end
declare class EnumGearType_INTERNAL extends Enum
	MeleeWeapons: EnumGearType
	RangedWeapons: EnumGearType
	Explosives: EnumGearType
	PowerUps: EnumGearType
	NavigationEnhancers: EnumGearType
	MusicalInstruments: EnumGearType
	SocialItems: EnumGearType
	BuildingTools: EnumGearType
	Transport: EnumGearType
end
declare class EnumGenre extends EnumItem end
declare class EnumGenre_INTERNAL extends Enum
	All: EnumGenre
	TownAndCity: EnumGenre
	Fantasy: EnumGenre
	SciFi: EnumGenre
	Ninja: EnumGenre
	Scary: EnumGenre
	Pirate: EnumGenre
	Adventure: EnumGenre
	Sports: EnumGenre
	Funny: EnumGenre
	WildWest: EnumGenre
	War: EnumGenre
	SkatePark: EnumGenre
	Tutorial: EnumGenre
end
declare class EnumGraphicsMode extends EnumItem end
declare class EnumGraphicsMode_INTERNAL extends Enum
	Automatic: EnumGraphicsMode
	Direct3D11: EnumGraphicsMode
	OpenGL: EnumGraphicsMode
	Metal: EnumGraphicsMode
	Vulkan: EnumGraphicsMode
	NoGraphics: EnumGraphicsMode
end
declare class EnumGuiType extends EnumItem end
declare class EnumGuiType_INTERNAL extends Enum
	Core: EnumGuiType
	Custom: EnumGuiType
	CustomBillboards: EnumGuiType
	PlayerNameplates: EnumGuiType
end
declare class EnumHandlesStyle extends EnumItem end
declare class EnumHandlesStyle_INTERNAL extends Enum
	Resize: EnumHandlesStyle
	Movement: EnumHandlesStyle
end
declare class EnumHighlightDepthMode extends EnumItem end
declare class EnumHighlightDepthMode_INTERNAL extends Enum
	AlwaysOnTop: EnumHighlightDepthMode
	Occluded: EnumHighlightDepthMode
end
declare class EnumHorizontalAlignment extends EnumItem end
declare class EnumHorizontalAlignment_INTERNAL extends Enum
	Center: EnumHorizontalAlignment
	Left: EnumHorizontalAlignment
	Right: EnumHorizontalAlignment
end
declare class EnumHoverAnimateSpeed extends EnumItem end
declare class EnumHoverAnimateSpeed_INTERNAL extends Enum
	VerySlow: EnumHoverAnimateSpeed
	Slow: EnumHoverAnimateSpeed
	Medium: EnumHoverAnimateSpeed
	Fast: EnumHoverAnimateSpeed
	VeryFast: EnumHoverAnimateSpeed
end
declare class EnumHttpCachePolicy extends EnumItem end
declare class EnumHttpCachePolicy_INTERNAL extends Enum
	None: EnumHttpCachePolicy
	Full: EnumHttpCachePolicy
	DataOnly: EnumHttpCachePolicy
	Default: EnumHttpCachePolicy
	InternalRedirectRefresh: EnumHttpCachePolicy
end
declare class EnumHttpContentType extends EnumItem end
declare class EnumHttpContentType_INTERNAL extends Enum
	ApplicationJson: EnumHttpContentType
	ApplicationXml: EnumHttpContentType
	ApplicationUrlEncoded: EnumHttpContentType
	TextPlain: EnumHttpContentType
	TextXml: EnumHttpContentType
end
declare class EnumHttpError extends EnumItem end
declare class EnumHttpError_INTERNAL extends Enum
	OK: EnumHttpError
	InvalidUrl: EnumHttpError
	DnsResolve: EnumHttpError
	ConnectFail: EnumHttpError
	OutOfMemory: EnumHttpError
	TimedOut: EnumHttpError
	TooManyRedirects: EnumHttpError
	InvalidRedirect: EnumHttpError
	NetFail: EnumHttpError
	Aborted: EnumHttpError
	SslConnectFail: EnumHttpError
	SslVerificationFail: EnumHttpError
	Unknown: EnumHttpError
end
declare class EnumHttpRequestType extends EnumItem end
declare class EnumHttpRequestType_INTERNAL extends Enum
	Default: EnumHttpRequestType
	MarketplaceService: EnumHttpRequestType
	Players: EnumHttpRequestType
	Chat: EnumHttpRequestType
	Avatar: EnumHttpRequestType
	Analytics: EnumHttpRequestType
	Localization: EnumHttpRequestType
end
declare class EnumHumanoidCollisionType extends EnumItem end
declare class EnumHumanoidCollisionType_INTERNAL extends Enum
	OuterBox: EnumHumanoidCollisionType
	InnerBox: EnumHumanoidCollisionType
end
declare class EnumHumanoidDisplayDistanceType extends EnumItem end
declare class EnumHumanoidDisplayDistanceType_INTERNAL extends Enum
	Viewer: EnumHumanoidDisplayDistanceType
	Subject: EnumHumanoidDisplayDistanceType
	None: EnumHumanoidDisplayDistanceType
end
declare class EnumHumanoidHealthDisplayType extends EnumItem end
declare class EnumHumanoidHealthDisplayType_INTERNAL extends Enum
	DisplayWhenDamaged: EnumHumanoidHealthDisplayType
	AlwaysOn: EnumHumanoidHealthDisplayType
	AlwaysOff: EnumHumanoidHealthDisplayType
end
declare class EnumHumanoidOnlySetCollisionsOnStateChange extends EnumItem end
declare class EnumHumanoidOnlySetCollisionsOnStateChange_INTERNAL extends Enum
	Default: EnumHumanoidOnlySetCollisionsOnStateChange
	Disabled: EnumHumanoidOnlySetCollisionsOnStateChange
	Enabled: EnumHumanoidOnlySetCollisionsOnStateChange
end
declare class EnumHumanoidRigType extends EnumItem end
declare class EnumHumanoidRigType_INTERNAL extends Enum
	R6: EnumHumanoidRigType
	R15: EnumHumanoidRigType
end
declare class EnumHumanoidStateMachineMode extends EnumItem end
declare class EnumHumanoidStateMachineMode_INTERNAL extends Enum
	Default: EnumHumanoidStateMachineMode
	Legacy: EnumHumanoidStateMachineMode
	NoStateMachine: EnumHumanoidStateMachineMode
	LuaStateMachine: EnumHumanoidStateMachineMode
end
declare class EnumHumanoidStateType extends EnumItem end
declare class EnumHumanoidStateType_INTERNAL extends Enum
	FallingDown: EnumHumanoidStateType
	Running: EnumHumanoidStateType
	RunningNoPhysics: EnumHumanoidStateType
	Climbing: EnumHumanoidStateType
	StrafingNoPhysics: EnumHumanoidStateType
	Ragdoll: EnumHumanoidStateType
	GettingUp: EnumHumanoidStateType
	Jumping: EnumHumanoidStateType
	Landed: EnumHumanoidStateType
	Flying: EnumHumanoidStateType
	Freefall: EnumHumanoidStateType
	Seated: EnumHumanoidStateType
	PlatformStanding: EnumHumanoidStateType
	Dead: EnumHumanoidStateType
	Swimming: EnumHumanoidStateType
	Physics: EnumHumanoidStateType
	None: EnumHumanoidStateType
end
declare class EnumIKCollisionsMode extends EnumItem end
declare class EnumIKCollisionsMode_INTERNAL extends Enum
	NoCollisions: EnumIKCollisionsMode
	OtherMechanismsAnchored: EnumIKCollisionsMode
	IncludeContactedMechanisms: EnumIKCollisionsMode
end
declare class EnumIKControlConstraintSupport extends EnumItem end
declare class EnumIKControlConstraintSupport_INTERNAL extends Enum
	Default: EnumIKControlConstraintSupport
	Disabled: EnumIKControlConstraintSupport
	Enabled: EnumIKControlConstraintSupport
end
declare class EnumIKControlType extends EnumItem end
declare class EnumIKControlType_INTERNAL extends Enum
	Transform: EnumIKControlType
	Position: EnumIKControlType
	Rotation: EnumIKControlType
	LookAt: EnumIKControlType
end
declare class EnumIXPLoadingStatus extends EnumItem end
declare class EnumIXPLoadingStatus_INTERNAL extends Enum
	None: EnumIXPLoadingStatus
	Pending: EnumIXPLoadingStatus
	Initialized: EnumIXPLoadingStatus
	ErrorTimedOut: EnumIXPLoadingStatus
	ErrorConnection: EnumIXPLoadingStatus
	ErrorJsonParse: EnumIXPLoadingStatus
	ErrorInvalidUser: EnumIXPLoadingStatus
end
declare class EnumInOut extends EnumItem end
declare class EnumInOut_INTERNAL extends Enum
	Edge: EnumInOut
	Inset: EnumInOut
	Center: EnumInOut
end
declare class EnumInfoType extends EnumItem end
declare class EnumInfoType_INTERNAL extends Enum
	Asset: EnumInfoType
	Product: EnumInfoType
	GamePass: EnumInfoType
	Subscription: EnumInfoType
	Bundle: EnumInfoType
end
declare class EnumInitialDockState extends EnumItem end
declare class EnumInitialDockState_INTERNAL extends Enum
	Top: EnumInitialDockState
	Bottom: EnumInitialDockState
	Left: EnumInitialDockState
	Right: EnumInitialDockState
	Float: EnumInitialDockState
end
declare class EnumInputType extends EnumItem end
declare class EnumInputType_INTERNAL extends Enum
	NoInput: EnumInputType
	Constant: EnumInputType
	Sin: EnumInputType
end
declare class EnumInterpolationThrottlingMode extends EnumItem end
declare class EnumInterpolationThrottlingMode_INTERNAL extends Enum
	Default: EnumInterpolationThrottlingMode
	Disabled: EnumInterpolationThrottlingMode
	Enabled: EnumInterpolationThrottlingMode
end
declare class EnumJointCreationMode extends EnumItem end
declare class EnumJointCreationMode_INTERNAL extends Enum
	All: EnumJointCreationMode
	Surface: EnumJointCreationMode
	None: EnumJointCreationMode
end
declare class EnumKeyCode extends EnumItem end
declare class EnumKeyCode_INTERNAL extends Enum
	Unknown: EnumKeyCode
	Backspace: EnumKeyCode
	Tab: EnumKeyCode
	Clear: EnumKeyCode
	Return: EnumKeyCode
	Pause: EnumKeyCode
	Escape: EnumKeyCode
	Space: EnumKeyCode
	QuotedDouble: EnumKeyCode
	Hash: EnumKeyCode
	Dollar: EnumKeyCode
	Percent: EnumKeyCode
	Ampersand: EnumKeyCode
	Quote: EnumKeyCode
	LeftParenthesis: EnumKeyCode
	RightParenthesis: EnumKeyCode
	Asterisk: EnumKeyCode
	Plus: EnumKeyCode
	Comma: EnumKeyCode
	Minus: EnumKeyCode
	Period: EnumKeyCode
	Slash: EnumKeyCode
	Zero: EnumKeyCode
	One: EnumKeyCode
	Two: EnumKeyCode
	Three: EnumKeyCode
	Four: EnumKeyCode
	Five: EnumKeyCode
	Six: EnumKeyCode
	Seven: EnumKeyCode
	Eight: EnumKeyCode
	Nine: EnumKeyCode
	Colon: EnumKeyCode
	Semicolon: EnumKeyCode
	LessThan: EnumKeyCode
	Equals: EnumKeyCode
	GreaterThan: EnumKeyCode
	Question: EnumKeyCode
	At: EnumKeyCode
	LeftBracket: EnumKeyCode
	BackSlash: EnumKeyCode
	RightBracket: EnumKeyCode
	Caret: EnumKeyCode
	Underscore: EnumKeyCode
	Backquote: EnumKeyCode
	A: EnumKeyCode
	B: EnumKeyCode
	C: EnumKeyCode
	D: EnumKeyCode
	E: EnumKeyCode
	F: EnumKeyCode
	G: EnumKeyCode
	H: EnumKeyCode
	I: EnumKeyCode
	J: EnumKeyCode
	K: EnumKeyCode
	L: EnumKeyCode
	M: EnumKeyCode
	N: EnumKeyCode
	O: EnumKeyCode
	P: EnumKeyCode
	Q: EnumKeyCode
	R: EnumKeyCode
	S: EnumKeyCode
	T: EnumKeyCode
	U: EnumKeyCode
	V: EnumKeyCode
	W: EnumKeyCode
	X: EnumKeyCode
	Y: EnumKeyCode
	Z: EnumKeyCode
	LeftCurly: EnumKeyCode
	Pipe: EnumKeyCode
	RightCurly: EnumKeyCode
	Tilde: EnumKeyCode
	Delete: EnumKeyCode
	KeypadZero: EnumKeyCode
	KeypadOne: EnumKeyCode
	KeypadTwo: EnumKeyCode
	KeypadThree: EnumKeyCode
	KeypadFour: EnumKeyCode
	KeypadFive: EnumKeyCode
	KeypadSix: EnumKeyCode
	KeypadSeven: EnumKeyCode
	KeypadEight: EnumKeyCode
	KeypadNine: EnumKeyCode
	KeypadPeriod: EnumKeyCode
	KeypadDivide: EnumKeyCode
	KeypadMultiply: EnumKeyCode
	KeypadMinus: EnumKeyCode
	KeypadPlus: EnumKeyCode
	KeypadEnter: EnumKeyCode
	KeypadEquals: EnumKeyCode
	Up: EnumKeyCode
	Down: EnumKeyCode
	Right: EnumKeyCode
	Left: EnumKeyCode
	Insert: EnumKeyCode
	Home: EnumKeyCode
	End: EnumKeyCode
	PageUp: EnumKeyCode
	PageDown: EnumKeyCode
	LeftShift: EnumKeyCode
	RightShift: EnumKeyCode
	LeftMeta: EnumKeyCode
	RightMeta: EnumKeyCode
	LeftAlt: EnumKeyCode
	RightAlt: EnumKeyCode
	LeftControl: EnumKeyCode
	RightControl: EnumKeyCode
	CapsLock: EnumKeyCode
	NumLock: EnumKeyCode
	ScrollLock: EnumKeyCode
	LeftSuper: EnumKeyCode
	RightSuper: EnumKeyCode
	Mode: EnumKeyCode
	Compose: EnumKeyCode
	Help: EnumKeyCode
	Print: EnumKeyCode
	SysReq: EnumKeyCode
	Break: EnumKeyCode
	Menu: EnumKeyCode
	Power: EnumKeyCode
	Euro: EnumKeyCode
	Undo: EnumKeyCode
	F1: EnumKeyCode
	F2: EnumKeyCode
	F3: EnumKeyCode
	F4: EnumKeyCode
	F5: EnumKeyCode
	F6: EnumKeyCode
	F7: EnumKeyCode
	F8: EnumKeyCode
	F9: EnumKeyCode
	F10: EnumKeyCode
	F11: EnumKeyCode
	F12: EnumKeyCode
	F13: EnumKeyCode
	F14: EnumKeyCode
	F15: EnumKeyCode
	World0: EnumKeyCode
	World1: EnumKeyCode
	World2: EnumKeyCode
	World3: EnumKeyCode
	World4: EnumKeyCode
	World5: EnumKeyCode
	World6: EnumKeyCode
	World7: EnumKeyCode
	World8: EnumKeyCode
	World9: EnumKeyCode
	World10: EnumKeyCode
	World11: EnumKeyCode
	World12: EnumKeyCode
	World13: EnumKeyCode
	World14: EnumKeyCode
	World15: EnumKeyCode
	World16: EnumKeyCode
	World17: EnumKeyCode
	World18: EnumKeyCode
	World19: EnumKeyCode
	World20: EnumKeyCode
	World21: EnumKeyCode
	World22: EnumKeyCode
	World23: EnumKeyCode
	World24: EnumKeyCode
	World25: EnumKeyCode
	World26: EnumKeyCode
	World27: EnumKeyCode
	World28: EnumKeyCode
	World29: EnumKeyCode
	World30: EnumKeyCode
	World31: EnumKeyCode
	World32: EnumKeyCode
	World33: EnumKeyCode
	World34: EnumKeyCode
	World35: EnumKeyCode
	World36: EnumKeyCode
	World37: EnumKeyCode
	World38: EnumKeyCode
	World39: EnumKeyCode
	World40: EnumKeyCode
	World41: EnumKeyCode
	World42: EnumKeyCode
	World43: EnumKeyCode
	World44: EnumKeyCode
	World45: EnumKeyCode
	World46: EnumKeyCode
	World47: EnumKeyCode
	World48: EnumKeyCode
	World49: EnumKeyCode
	World50: EnumKeyCode
	World51: EnumKeyCode
	World52: EnumKeyCode
	World53: EnumKeyCode
	World54: EnumKeyCode
	World55: EnumKeyCode
	World56: EnumKeyCode
	World57: EnumKeyCode
	World58: EnumKeyCode
	World59: EnumKeyCode
	World60: EnumKeyCode
	World61: EnumKeyCode
	World62: EnumKeyCode
	World63: EnumKeyCode
	World64: EnumKeyCode
	World65: EnumKeyCode
	World66: EnumKeyCode
	World67: EnumKeyCode
	World68: EnumKeyCode
	World69: EnumKeyCode
	World70: EnumKeyCode
	World71: EnumKeyCode
	World72: EnumKeyCode
	World73: EnumKeyCode
	World74: EnumKeyCode
	World75: EnumKeyCode
	World76: EnumKeyCode
	World77: EnumKeyCode
	World78: EnumKeyCode
	World79: EnumKeyCode
	World80: EnumKeyCode
	World81: EnumKeyCode
	World82: EnumKeyCode
	World83: EnumKeyCode
	World84: EnumKeyCode
	World85: EnumKeyCode
	World86: EnumKeyCode
	World87: EnumKeyCode
	World88: EnumKeyCode
	World89: EnumKeyCode
	World90: EnumKeyCode
	World91: EnumKeyCode
	World92: EnumKeyCode
	World93: EnumKeyCode
	World94: EnumKeyCode
	World95: EnumKeyCode
	ButtonX: EnumKeyCode
	ButtonY: EnumKeyCode
	ButtonA: EnumKeyCode
	ButtonB: EnumKeyCode
	ButtonR1: EnumKeyCode
	ButtonL1: EnumKeyCode
	ButtonR2: EnumKeyCode
	ButtonL2: EnumKeyCode
	ButtonR3: EnumKeyCode
	ButtonL3: EnumKeyCode
	ButtonStart: EnumKeyCode
	ButtonSelect: EnumKeyCode
	DPadLeft: EnumKeyCode
	DPadRight: EnumKeyCode
	DPadUp: EnumKeyCode
	DPadDown: EnumKeyCode
	Thumbstick1: EnumKeyCode
	Thumbstick2: EnumKeyCode
end
declare class EnumKeyInterpolationMode extends EnumItem end
declare class EnumKeyInterpolationMode_INTERNAL extends Enum
	Constant: EnumKeyInterpolationMode
	Linear: EnumKeyInterpolationMode
	Cubic: EnumKeyInterpolationMode
end
declare class EnumKeywordFilterType extends EnumItem end
declare class EnumKeywordFilterType_INTERNAL extends Enum
	Include: EnumKeywordFilterType
	Exclude: EnumKeywordFilterType
end
declare class EnumLanguage extends EnumItem end
declare class EnumLanguage_INTERNAL extends Enum
	Default: EnumLanguage
end
declare class EnumLeftRight extends EnumItem end
declare class EnumLeftRight_INTERNAL extends Enum
	Left: EnumLeftRight
	Center: EnumLeftRight
	Right: EnumLeftRight
end
declare class EnumLimb extends EnumItem end
declare class EnumLimb_INTERNAL extends Enum
	Head: EnumLimb
	Torso: EnumLimb
	LeftArm: EnumLimb
	RightArm: EnumLimb
	LeftLeg: EnumLimb
	RightLeg: EnumLimb
	Unknown: EnumLimb
end
declare class EnumLineJoinMode extends EnumItem end
declare class EnumLineJoinMode_INTERNAL extends Enum
	Round: EnumLineJoinMode
	Bevel: EnumLineJoinMode
	Miter: EnumLineJoinMode
end
declare class EnumListDisplayMode extends EnumItem end
declare class EnumListDisplayMode_INTERNAL extends Enum
	Horizontal: EnumListDisplayMode
	Vertical: EnumListDisplayMode
end
declare class EnumListenerType extends EnumItem end
declare class EnumListenerType_INTERNAL extends Enum
	Camera: EnumListenerType
	CFrame: EnumListenerType
	ObjectPosition: EnumListenerType
	ObjectCFrame: EnumListenerType
end
declare class EnumLoadCharacterLayeredClothing extends EnumItem end
declare class EnumLoadCharacterLayeredClothing_INTERNAL extends Enum
	Default: EnumLoadCharacterLayeredClothing
	Disabled: EnumLoadCharacterLayeredClothing
	Enabled: EnumLoadCharacterLayeredClothing
end
declare class EnumLoadDynamicHeads extends EnumItem end
declare class EnumLoadDynamicHeads_INTERNAL extends Enum
	Default: EnumLoadDynamicHeads
	Disabled: EnumLoadDynamicHeads
	Enabled: EnumLoadDynamicHeads
end
declare class EnumMarkupKind extends EnumItem end
declare class EnumMarkupKind_INTERNAL extends Enum
	PlainText: EnumMarkupKind
	Markdown: EnumMarkupKind
end
declare class EnumMaterial extends EnumItem end
declare class EnumMaterial_INTERNAL extends Enum
	Plastic: EnumMaterial
	Wood: EnumMaterial
	Slate: EnumMaterial
	Concrete: EnumMaterial
	CorrodedMetal: EnumMaterial
	DiamondPlate: EnumMaterial
	Foil: EnumMaterial
	Grass: EnumMaterial
	Ice: EnumMaterial
	Marble: EnumMaterial
	Granite: EnumMaterial
	Brick: EnumMaterial
	Pebble: EnumMaterial
	Sand: EnumMaterial
	Fabric: EnumMaterial
	SmoothPlastic: EnumMaterial
	Metal: EnumMaterial
	WoodPlanks: EnumMaterial
	Cobblestone: EnumMaterial
	Air: EnumMaterial
	Water: EnumMaterial
	Rock: EnumMaterial
	Glacier: EnumMaterial
	Snow: EnumMaterial
	Sandstone: EnumMaterial
	Mud: EnumMaterial
	Basalt: EnumMaterial
	Ground: EnumMaterial
	CrackedLava: EnumMaterial
	Neon: EnumMaterial
	Glass: EnumMaterial
	Asphalt: EnumMaterial
	LeafyGrass: EnumMaterial
	Salt: EnumMaterial
	Limestone: EnumMaterial
	Pavement: EnumMaterial
	ForceField: EnumMaterial
end
declare class EnumMaterialPattern extends EnumItem end
declare class EnumMaterialPattern_INTERNAL extends Enum
	Regular: EnumMaterialPattern
	Organic: EnumMaterialPattern
end
declare class EnumMembershipType extends EnumItem end
declare class EnumMembershipType_INTERNAL extends Enum
	None: EnumMembershipType
	BuildersClub: EnumMembershipType
	TurboBuildersClub: EnumMembershipType
	OutrageousBuildersClub: EnumMembershipType
	Premium: EnumMembershipType
end
declare class EnumMeshPartDetailLevel extends EnumItem end
declare class EnumMeshPartDetailLevel_INTERNAL extends Enum
	DistanceBased: EnumMeshPartDetailLevel
	Level00: EnumMeshPartDetailLevel
	Level01: EnumMeshPartDetailLevel
	Level02: EnumMeshPartDetailLevel
	Level03: EnumMeshPartDetailLevel
	Level04: EnumMeshPartDetailLevel
end
declare class EnumMeshPartHeadsAndAccessories extends EnumItem end
declare class EnumMeshPartHeadsAndAccessories_INTERNAL extends Enum
	Default: EnumMeshPartHeadsAndAccessories
	Disabled: EnumMeshPartHeadsAndAccessories
	Enabled: EnumMeshPartHeadsAndAccessories
end
declare class EnumMeshScaleUnit extends EnumItem end
declare class EnumMeshScaleUnit_INTERNAL extends Enum
	Stud: EnumMeshScaleUnit
	Meter: EnumMeshScaleUnit
	CM: EnumMeshScaleUnit
	MM: EnumMeshScaleUnit
	Foot: EnumMeshScaleUnit
	Inch: EnumMeshScaleUnit
end
declare class EnumMeshType extends EnumItem end
declare class EnumMeshType_INTERNAL extends Enum
	Head: EnumMeshType
	Torso: EnumMeshType
	Wedge: EnumMeshType
	Prism: EnumMeshType
	Pyramid: EnumMeshType
	ParallelRamp: EnumMeshType
	RightAngleRamp: EnumMeshType
	CornerWedge: EnumMeshType
	Brick: EnumMeshType
	Sphere: EnumMeshType
	Cylinder: EnumMeshType
	FileMesh: EnumMeshType
end
declare class EnumMessageType extends EnumItem end
declare class EnumMessageType_INTERNAL extends Enum
	MessageOutput: EnumMessageType
	MessageInfo: EnumMessageType
	MessageWarning: EnumMessageType
	MessageError: EnumMessageType
end
declare class EnumModelLevelOfDetail extends EnumItem end
declare class EnumModelLevelOfDetail_INTERNAL extends Enum
	Automatic: EnumModelLevelOfDetail
	StreamingMesh: EnumModelLevelOfDetail
	Disabled: EnumModelLevelOfDetail
end
declare class EnumModelStreamingBehavior extends EnumItem end
declare class EnumModelStreamingBehavior_INTERNAL extends Enum
	Default: EnumModelStreamingBehavior
	Legacy: EnumModelStreamingBehavior
	Improved: EnumModelStreamingBehavior
end
declare class EnumModelStreamingMode extends EnumItem end
declare class EnumModelStreamingMode_INTERNAL extends Enum
	Default: EnumModelStreamingMode
	Atomic: EnumModelStreamingMode
	Persistent: EnumModelStreamingMode
	PersistentPerPlayer: EnumModelStreamingMode
	Nonatomic: EnumModelStreamingMode
end
declare class EnumModifierKey extends EnumItem end
declare class EnumModifierKey_INTERNAL extends Enum
	Alt: EnumModifierKey
	Ctrl: EnumModifierKey
	Meta: EnumModifierKey
	Shift: EnumModifierKey
end
declare class EnumMouseBehavior extends EnumItem end
declare class EnumMouseBehavior_INTERNAL extends Enum
	Default: EnumMouseBehavior
	LockCenter: EnumMouseBehavior
	LockCurrentPosition: EnumMouseBehavior
end
declare class EnumMoveState extends EnumItem end
declare class EnumMoveState_INTERNAL extends Enum
	Stopped: EnumMoveState
	Coasting: EnumMoveState
	Pushing: EnumMoveState
	Stopping: EnumMoveState
	AirFree: EnumMoveState
end
declare class EnumMuteState extends EnumItem end
declare class EnumMuteState_INTERNAL extends Enum
	Unmuted: EnumMuteState
	Muted: EnumMuteState
end
declare class EnumNameOcclusion extends EnumItem end
declare class EnumNameOcclusion_INTERNAL extends Enum
	OccludeAll: EnumNameOcclusion
	EnemyOcclusion: EnumNameOcclusion
	NoOcclusion: EnumNameOcclusion
end
declare class EnumNetworkOwnership extends EnumItem end
declare class EnumNetworkOwnership_INTERNAL extends Enum
	Automatic: EnumNetworkOwnership
	Manual: EnumNetworkOwnership
	OnContact: EnumNetworkOwnership
end
declare class EnumNormalId extends EnumItem end
declare class EnumNormalId_INTERNAL extends Enum
	Top: EnumNormalId
	Bottom: EnumNormalId
	Back: EnumNormalId
	Front: EnumNormalId
	Right: EnumNormalId
	Left: EnumNormalId
end
declare class EnumOrientationAlignmentMode extends EnumItem end
declare class EnumOrientationAlignmentMode_INTERNAL extends Enum
	OneAttachment: EnumOrientationAlignmentMode
	TwoAttachment: EnumOrientationAlignmentMode
end
declare class EnumOutfitSource extends EnumItem end
declare class EnumOutfitSource_INTERNAL extends Enum
	All: EnumOutfitSource
	Created: EnumOutfitSource
	Purchased: EnumOutfitSource
end
declare class EnumOutfitType extends EnumItem end
declare class EnumOutfitType_INTERNAL extends Enum
	All: EnumOutfitType
	Avatar: EnumOutfitType
	DynamicHead: EnumOutfitType
end
declare class EnumOutputLayoutMode extends EnumItem end
declare class EnumOutputLayoutMode_INTERNAL extends Enum
	Horizontal: EnumOutputLayoutMode
	Vertical: EnumOutputLayoutMode
end
declare class EnumOverrideMouseIconBehavior extends EnumItem end
declare class EnumOverrideMouseIconBehavior_INTERNAL extends Enum
	None: EnumOverrideMouseIconBehavior
	ForceShow: EnumOverrideMouseIconBehavior
	ForceHide: EnumOverrideMouseIconBehavior
end
declare class EnumPackagePermission extends EnumItem end
declare class EnumPackagePermission_INTERNAL extends Enum
	None: EnumPackagePermission
	NoAccess: EnumPackagePermission
	Revoked: EnumPackagePermission
	UseView: EnumPackagePermission
	Edit: EnumPackagePermission
	Own: EnumPackagePermission
end
declare class EnumPartType extends EnumItem end
declare class EnumPartType_INTERNAL extends Enum
	Ball: EnumPartType
	Block: EnumPartType
	Cylinder: EnumPartType
	Wedge: EnumPartType
	CornerWedge: EnumPartType
end
declare class EnumParticleEmitterShape extends EnumItem end
declare class EnumParticleEmitterShape_INTERNAL extends Enum
	Box: EnumParticleEmitterShape
	Sphere: EnumParticleEmitterShape
	Cylinder: EnumParticleEmitterShape
	Disc: EnumParticleEmitterShape
end
declare class EnumParticleEmitterShapeInOut extends EnumItem end
declare class EnumParticleEmitterShapeInOut_INTERNAL extends Enum
	Outward: EnumParticleEmitterShapeInOut
	Inward: EnumParticleEmitterShapeInOut
	InAndOut: EnumParticleEmitterShapeInOut
end
declare class EnumParticleEmitterShapeStyle extends EnumItem end
declare class EnumParticleEmitterShapeStyle_INTERNAL extends Enum
	Volume: EnumParticleEmitterShapeStyle
	Surface: EnumParticleEmitterShapeStyle
end
declare class EnumParticleFlipbookLayout extends EnumItem end
declare class EnumParticleFlipbookLayout_INTERNAL extends Enum
	None: EnumParticleFlipbookLayout
	Grid2x2: EnumParticleFlipbookLayout
	Grid4x4: EnumParticleFlipbookLayout
	Grid8x8: EnumParticleFlipbookLayout
end
declare class EnumParticleFlipbookMode extends EnumItem end
declare class EnumParticleFlipbookMode_INTERNAL extends Enum
	Loop: EnumParticleFlipbookMode
	OneShot: EnumParticleFlipbookMode
	PingPong: EnumParticleFlipbookMode
	Random: EnumParticleFlipbookMode
end
declare class EnumParticleFlipbookTextureCompatible extends EnumItem end
declare class EnumParticleFlipbookTextureCompatible_INTERNAL extends Enum
	NotCompatible: EnumParticleFlipbookTextureCompatible
	Compatible: EnumParticleFlipbookTextureCompatible
	Unknown: EnumParticleFlipbookTextureCompatible
end
declare class EnumParticleOrientation extends EnumItem end
declare class EnumParticleOrientation_INTERNAL extends Enum
	FacingCamera: EnumParticleOrientation
	FacingCameraWorldUp: EnumParticleOrientation
	VelocityParallel: EnumParticleOrientation
	VelocityPerpendicular: EnumParticleOrientation
end
declare class EnumPathStatus extends EnumItem end
declare class EnumPathStatus_INTERNAL extends Enum
	Success: EnumPathStatus
	ClosestNoPath: EnumPathStatus
	ClosestOutOfRange: EnumPathStatus
	FailStartNotEmpty: EnumPathStatus
	FailFinishNotEmpty: EnumPathStatus
	NoPath: EnumPathStatus
end
declare class EnumPathWaypointAction extends EnumItem end
declare class EnumPathWaypointAction_INTERNAL extends Enum
	Walk: EnumPathWaypointAction
	Jump: EnumPathWaypointAction
	Custom: EnumPathWaypointAction
end
declare class EnumPermissionLevelShown extends EnumItem end
declare class EnumPermissionLevelShown_INTERNAL extends Enum
	Game: EnumPermissionLevelShown
	RobloxGame: EnumPermissionLevelShown
	RobloxScript: EnumPermissionLevelShown
	Studio: EnumPermissionLevelShown
	Roblox: EnumPermissionLevelShown
end
declare class EnumPhysicsSimulationRate extends EnumItem end
declare class EnumPhysicsSimulationRate_INTERNAL extends Enum
	Fixed240Hz: EnumPhysicsSimulationRate
	Fixed120Hz: EnumPhysicsSimulationRate
	Fixed60Hz: EnumPhysicsSimulationRate
end
declare class EnumPhysicsSteppingMethod extends EnumItem end
declare class EnumPhysicsSteppingMethod_INTERNAL extends Enum
	Default: EnumPhysicsSteppingMethod
	Fixed: EnumPhysicsSteppingMethod
	Adaptive: EnumPhysicsSteppingMethod
end
declare class EnumPlatform extends EnumItem end
declare class EnumPlatform_INTERNAL extends Enum
	Windows: EnumPlatform
	OSX: EnumPlatform
	IOS: EnumPlatform
	Android: EnumPlatform
	XBoxOne: EnumPlatform
	PS4: EnumPlatform
	PS3: EnumPlatform
	XBox360: EnumPlatform
	WiiU: EnumPlatform
	NX: EnumPlatform
	Ouya: EnumPlatform
	AndroidTV: EnumPlatform
	Chromecast: EnumPlatform
	Linux: EnumPlatform
	SteamOS: EnumPlatform
	WebOS: EnumPlatform
	DOS: EnumPlatform
	BeOS: EnumPlatform
	UWP: EnumPlatform
	None: EnumPlatform
end
declare class EnumPlaybackState extends EnumItem end
declare class EnumPlaybackState_INTERNAL extends Enum
	Begin: EnumPlaybackState
	Delayed: EnumPlaybackState
	Playing: EnumPlaybackState
	Paused: EnumPlaybackState
	Completed: EnumPlaybackState
	Cancelled: EnumPlaybackState
end
declare class EnumPlayerActions extends EnumItem end
declare class EnumPlayerActions_INTERNAL extends Enum
	CharacterForward: EnumPlayerActions
	CharacterBackward: EnumPlayerActions
	CharacterLeft: EnumPlayerActions
	CharacterRight: EnumPlayerActions
	CharacterJump: EnumPlayerActions
end
declare class EnumPlayerChatType extends EnumItem end
declare class EnumPlayerChatType_INTERNAL extends Enum
	All: EnumPlayerChatType
	Team: EnumPlayerChatType
	Whisper: EnumPlayerChatType
end
declare class EnumPoseEasingDirection extends EnumItem end
declare class EnumPoseEasingDirection_INTERNAL extends Enum
	Out: EnumPoseEasingDirection
	InOut: EnumPoseEasingDirection
	In: EnumPoseEasingDirection
end
declare class EnumPoseEasingStyle extends EnumItem end
declare class EnumPoseEasingStyle_INTERNAL extends Enum
	Linear: EnumPoseEasingStyle
	Constant: EnumPoseEasingStyle
	Elastic: EnumPoseEasingStyle
	Cubic: EnumPoseEasingStyle
	Bounce: EnumPoseEasingStyle
end
declare class EnumPositionAlignmentMode extends EnumItem end
declare class EnumPositionAlignmentMode_INTERNAL extends Enum
	OneAttachment: EnumPositionAlignmentMode
	TwoAttachment: EnumPositionAlignmentMode
end
declare class EnumPrivilegeType extends EnumItem end
declare class EnumPrivilegeType_INTERNAL extends Enum
	Owner: EnumPrivilegeType
	Admin: EnumPrivilegeType
	Member: EnumPrivilegeType
	Visitor: EnumPrivilegeType
	Banned: EnumPrivilegeType
end
declare class EnumProductLocationRestriction extends EnumItem end
declare class EnumProductLocationRestriction_INTERNAL extends Enum
	AvatarShop: EnumProductLocationRestriction
	AllowedGames: EnumProductLocationRestriction
	AllGames: EnumProductLocationRestriction
end
declare class EnumProductPurchaseDecision extends EnumItem end
declare class EnumProductPurchaseDecision_INTERNAL extends Enum
	NotProcessedYet: EnumProductPurchaseDecision
	PurchaseGranted: EnumProductPurchaseDecision
end
declare class EnumPromptCreateAssetResult extends EnumItem end
declare class EnumPromptCreateAssetResult_INTERNAL extends Enum
	Success: EnumPromptCreateAssetResult
	PermissionDenied: EnumPromptCreateAssetResult
	Timeout: EnumPromptCreateAssetResult
	UploadFailed: EnumPromptCreateAssetResult
	NoUserInput: EnumPromptCreateAssetResult
	UnknownFailure: EnumPromptCreateAssetResult
end
declare class EnumPromptPublishAssetResult extends EnumItem end
declare class EnumPromptPublishAssetResult_INTERNAL extends Enum
	Success: EnumPromptPublishAssetResult
	PermissionDenied: EnumPromptPublishAssetResult
	Timeout: EnumPromptPublishAssetResult
	UploadFailed: EnumPromptPublishAssetResult
	NoUserInput: EnumPromptPublishAssetResult
	UnknownFailure: EnumPromptPublishAssetResult
end
declare class EnumPropertyStatus extends EnumItem end
declare class EnumPropertyStatus_INTERNAL extends Enum
	Ok: EnumPropertyStatus
	Warning: EnumPropertyStatus
	Error: EnumPropertyStatus
end
declare class EnumProximityPromptExclusivity extends EnumItem end
declare class EnumProximityPromptExclusivity_INTERNAL extends Enum
	OnePerButton: EnumProximityPromptExclusivity
	OneGlobally: EnumProximityPromptExclusivity
	AlwaysShow: EnumProximityPromptExclusivity
end
declare class EnumProximityPromptInputType extends EnumItem end
declare class EnumProximityPromptInputType_INTERNAL extends Enum
	Keyboard: EnumProximityPromptInputType
	Gamepad: EnumProximityPromptInputType
	Touch: EnumProximityPromptInputType
end
declare class EnumProximityPromptStyle extends EnumItem end
declare class EnumProximityPromptStyle_INTERNAL extends Enum
	Default: EnumProximityPromptStyle
	Custom: EnumProximityPromptStyle
end
declare class EnumQualityLevel extends EnumItem end
declare class EnumQualityLevel_INTERNAL extends Enum
	Automatic: EnumQualityLevel
	Level01: EnumQualityLevel
	Level02: EnumQualityLevel
	Level03: EnumQualityLevel
	Level04: EnumQualityLevel
	Level05: EnumQualityLevel
	Level06: EnumQualityLevel
	Level07: EnumQualityLevel
	Level08: EnumQualityLevel
	Level09: EnumQualityLevel
	Level10: EnumQualityLevel
	Level11: EnumQualityLevel
	Level12: EnumQualityLevel
	Level13: EnumQualityLevel
	Level14: EnumQualityLevel
	Level15: EnumQualityLevel
	Level16: EnumQualityLevel
	Level17: EnumQualityLevel
	Level18: EnumQualityLevel
	Level19: EnumQualityLevel
	Level20: EnumQualityLevel
	Level21: EnumQualityLevel
end
declare class EnumR15CollisionType extends EnumItem end
declare class EnumR15CollisionType_INTERNAL extends Enum
	OuterBox: EnumR15CollisionType
	InnerBox: EnumR15CollisionType
end
declare class EnumRaycastFilterType extends EnumItem end
declare class EnumRaycastFilterType_INTERNAL extends Enum
	Exclude: EnumRaycastFilterType
	Include: EnumRaycastFilterType
end
declare class EnumRejectCharacterDeletions extends EnumItem end
declare class EnumRejectCharacterDeletions_INTERNAL extends Enum
	Default: EnumRejectCharacterDeletions
	Disabled: EnumRejectCharacterDeletions
	Enabled: EnumRejectCharacterDeletions
end
declare class EnumRenderFidelity extends EnumItem end
declare class EnumRenderFidelity_INTERNAL extends Enum
	Automatic: EnumRenderFidelity
	Precise: EnumRenderFidelity
	Performance: EnumRenderFidelity
end
declare class EnumRenderPriority extends EnumItem end
declare class EnumRenderPriority_INTERNAL extends Enum
	First: EnumRenderPriority
	Input: EnumRenderPriority
	Camera: EnumRenderPriority
	Character: EnumRenderPriority
	Last: EnumRenderPriority
end
declare class EnumRenderingTestComparisonMethod extends EnumItem end
declare class EnumRenderingTestComparisonMethod_INTERNAL extends Enum
	psnr: EnumRenderingTestComparisonMethod
	diff: EnumRenderingTestComparisonMethod
end
declare class EnumReplicateInstanceDestroySetting extends EnumItem end
declare class EnumReplicateInstanceDestroySetting_INTERNAL extends Enum
	Default: EnumReplicateInstanceDestroySetting
	Disabled: EnumReplicateInstanceDestroySetting
	Enabled: EnumReplicateInstanceDestroySetting
end
declare class EnumResamplerMode extends EnumItem end
declare class EnumResamplerMode_INTERNAL extends Enum
	Default: EnumResamplerMode
	Pixelated: EnumResamplerMode
end
declare class EnumReservedHighlightId extends EnumItem end
declare class EnumReservedHighlightId_INTERNAL extends Enum
	Standard: EnumReservedHighlightId
	Selection: EnumReservedHighlightId
	Hover: EnumReservedHighlightId
	Active: EnumReservedHighlightId
end
declare class EnumRestPose extends EnumItem end
declare class EnumRestPose_INTERNAL extends Enum
	Default: EnumRestPose
	RotationsReset: EnumRestPose
	Custom: EnumRestPose
end
declare class EnumReturnKeyType extends EnumItem end
declare class EnumReturnKeyType_INTERNAL extends Enum
	Default: EnumReturnKeyType
	Done: EnumReturnKeyType
	Go: EnumReturnKeyType
	Next: EnumReturnKeyType
	Search: EnumReturnKeyType
	Send: EnumReturnKeyType
end
declare class EnumReverbType extends EnumItem end
declare class EnumReverbType_INTERNAL extends Enum
	NoReverb: EnumReverbType
	GenericReverb: EnumReverbType
	PaddedCell: EnumReverbType
	Room: EnumReverbType
	Bathroom: EnumReverbType
	LivingRoom: EnumReverbType
	StoneRoom: EnumReverbType
	Auditorium: EnumReverbType
	ConcertHall: EnumReverbType
	Cave: EnumReverbType
	Arena: EnumReverbType
	Hangar: EnumReverbType
	CarpettedHallway: EnumReverbType
	Hallway: EnumReverbType
	StoneCorridor: EnumReverbType
	Alley: EnumReverbType
	Forest: EnumReverbType
	City: EnumReverbType
	Mountains: EnumReverbType
	Quarry: EnumReverbType
	Plain: EnumReverbType
	ParkingLot: EnumReverbType
	SewerPipe: EnumReverbType
	UnderWater: EnumReverbType
end
declare class EnumRibbonTool extends EnumItem end
declare class EnumRibbonTool_INTERNAL extends Enum
	Select: EnumRibbonTool
	Scale: EnumRibbonTool
	Rotate: EnumRibbonTool
	Move: EnumRibbonTool
	Transform: EnumRibbonTool
	ColorPicker: EnumRibbonTool
	MaterialPicker: EnumRibbonTool
	Group: EnumRibbonTool
	Ungroup: EnumRibbonTool
	None: EnumRibbonTool
end
declare class EnumRigScale extends EnumItem end
declare class EnumRigScale_INTERNAL extends Enum
	Default: EnumRigScale
	Rthro: EnumRigScale
	RthroNarrow: EnumRigScale
end
declare class EnumRigType extends EnumItem end
declare class EnumRigType_INTERNAL extends Enum
	R15: EnumRigType
	Custom: EnumRigType
	None: EnumRigType
end
declare class EnumRollOffMode extends EnumItem end
declare class EnumRollOffMode_INTERNAL extends Enum
	Inverse: EnumRollOffMode
	Linear: EnumRollOffMode
	InverseTapered: EnumRollOffMode
	LinearSquare: EnumRollOffMode
end
declare class EnumRotationOrder extends EnumItem end
declare class EnumRotationOrder_INTERNAL extends Enum
	XYZ: EnumRotationOrder
	XZY: EnumRotationOrder
	YZX: EnumRotationOrder
	YXZ: EnumRotationOrder
	ZXY: EnumRotationOrder
	ZYX: EnumRotationOrder
end
declare class EnumRotationType extends EnumItem end
declare class EnumRotationType_INTERNAL extends Enum
	MovementRelative: EnumRotationType
	CameraRelative: EnumRotationType
end
declare class EnumRtlTextSupport extends EnumItem end
declare class EnumRtlTextSupport_INTERNAL extends Enum
	Default: EnumRtlTextSupport
	Disabled: EnumRtlTextSupport
	Enabled: EnumRtlTextSupport
end
declare class EnumRunContext extends EnumItem end
declare class EnumRunContext_INTERNAL extends Enum
	Legacy: EnumRunContext
	Server: EnumRunContext
	Client: EnumRunContext
	Plugin: EnumRunContext
end
declare class EnumRuntimeUndoBehavior extends EnumItem end
declare class EnumRuntimeUndoBehavior_INTERNAL extends Enum
	Aggregate: EnumRuntimeUndoBehavior
	Snapshot: EnumRuntimeUndoBehavior
	Hybrid: EnumRuntimeUndoBehavior
end
declare class EnumSafeAreaCompatibility extends EnumItem end
declare class EnumSafeAreaCompatibility_INTERNAL extends Enum
	None: EnumSafeAreaCompatibility
	FullscreenExtension: EnumSafeAreaCompatibility
end
declare class EnumSalesTypeFilter extends EnumItem end
declare class EnumSalesTypeFilter_INTERNAL extends Enum
	All: EnumSalesTypeFilter
	Collectibles: EnumSalesTypeFilter
	Premium: EnumSalesTypeFilter
end
declare class EnumSaveAvatarThumbnailCustomizationFailure extends EnumItem end
declare class EnumSaveAvatarThumbnailCustomizationFailure_INTERNAL extends Enum
	BadThumbnailType: EnumSaveAvatarThumbnailCustomizationFailure
	BadYRotDeg: EnumSaveAvatarThumbnailCustomizationFailure
	BadFieldOfViewDeg: EnumSaveAvatarThumbnailCustomizationFailure
	BadDistanceScale: EnumSaveAvatarThumbnailCustomizationFailure
	Other: EnumSaveAvatarThumbnailCustomizationFailure
end
declare class EnumSaveFilter extends EnumItem end
declare class EnumSaveFilter_INTERNAL extends Enum
	SaveAll: EnumSaveFilter
	SaveWorld: EnumSaveFilter
	SaveGame: EnumSaveFilter
end
declare class EnumSavedQualitySetting extends EnumItem end
declare class EnumSavedQualitySetting_INTERNAL extends Enum
	Automatic: EnumSavedQualitySetting
	QualityLevel1: EnumSavedQualitySetting
	QualityLevel2: EnumSavedQualitySetting
	QualityLevel3: EnumSavedQualitySetting
	QualityLevel4: EnumSavedQualitySetting
	QualityLevel5: EnumSavedQualitySetting
	QualityLevel6: EnumSavedQualitySetting
	QualityLevel7: EnumSavedQualitySetting
	QualityLevel8: EnumSavedQualitySetting
	QualityLevel9: EnumSavedQualitySetting
	QualityLevel10: EnumSavedQualitySetting
end
declare class EnumScaleType extends EnumItem end
declare class EnumScaleType_INTERNAL extends Enum
	Stretch: EnumScaleType
	Slice: EnumScaleType
	Tile: EnumScaleType
	Fit: EnumScaleType
	Crop: EnumScaleType
end
declare class EnumScopeCheckResult extends EnumItem end
declare class EnumScopeCheckResult_INTERNAL extends Enum
	ConsentAccepted: EnumScopeCheckResult
	InvalidScopes: EnumScopeCheckResult
	Timeout: EnumScopeCheckResult
	NoUserInput: EnumScopeCheckResult
	BackendError: EnumScopeCheckResult
	UnexpectedError: EnumScopeCheckResult
	InvalidArgument: EnumScopeCheckResult
	ConsentDenied: EnumScopeCheckResult
end
declare class EnumScreenInsets extends EnumItem end
declare class EnumScreenInsets_INTERNAL extends Enum
	None: EnumScreenInsets
	DeviceSafeInsets: EnumScreenInsets
	CoreUISafeInsets: EnumScreenInsets
end
declare class EnumScreenOrientation extends EnumItem end
declare class EnumScreenOrientation_INTERNAL extends Enum
	LandscapeLeft: EnumScreenOrientation
	LandscapeRight: EnumScreenOrientation
	LandscapeSensor: EnumScreenOrientation
	Portrait: EnumScreenOrientation
	Sensor: EnumScreenOrientation
end
declare class EnumScrollBarInset extends EnumItem end
declare class EnumScrollBarInset_INTERNAL extends Enum
	None: EnumScrollBarInset
	ScrollBar: EnumScrollBarInset
	Always: EnumScrollBarInset
end
declare class EnumScrollingDirection extends EnumItem end
declare class EnumScrollingDirection_INTERNAL extends Enum
	X: EnumScrollingDirection
	Y: EnumScrollingDirection
	XY: EnumScrollingDirection
end
declare class EnumSelectionBehavior extends EnumItem end
declare class EnumSelectionBehavior_INTERNAL extends Enum
	Escape: EnumSelectionBehavior
	Stop: EnumSelectionBehavior
end
declare class EnumSelectionRenderMode extends EnumItem end
declare class EnumSelectionRenderMode_INTERNAL extends Enum
	Outlines: EnumSelectionRenderMode
	BoundingBoxes: EnumSelectionRenderMode
	Both: EnumSelectionRenderMode
end
declare class EnumSelfViewPosition extends EnumItem end
declare class EnumSelfViewPosition_INTERNAL extends Enum
	LastPosition: EnumSelfViewPosition
	TopLeft: EnumSelfViewPosition
	TopRight: EnumSelfViewPosition
	BottomLeft: EnumSelfViewPosition
	BottomRight: EnumSelfViewPosition
end
declare class EnumSensorMode extends EnumItem end
declare class EnumSensorMode_INTERNAL extends Enum
	Floor: EnumSensorMode
	Ladder: EnumSensorMode
end
declare class EnumSensorUpdateType extends EnumItem end
declare class EnumSensorUpdateType_INTERNAL extends Enum
	OnRead: EnumSensorUpdateType
	Manual: EnumSensorUpdateType
end
declare class EnumServerAudioBehavior extends EnumItem end
declare class EnumServerAudioBehavior_INTERNAL extends Enum
	Enabled: EnumServerAudioBehavior
	Muted: EnumServerAudioBehavior
	OnlineGame: EnumServerAudioBehavior
end
declare class EnumServiceVisibility extends EnumItem end
declare class EnumServiceVisibility_INTERNAL extends Enum
	Always: EnumServiceVisibility
	Off: EnumServiceVisibility
	WithChildren: EnumServiceVisibility
end
declare class EnumSeverity extends EnumItem end
declare class EnumSeverity_INTERNAL extends Enum
	Error: EnumSeverity
	Warning: EnumSeverity
	Information: EnumSeverity
	Hint: EnumSeverity
end
declare class EnumSignalBehavior extends EnumItem end
declare class EnumSignalBehavior_INTERNAL extends Enum
	Default: EnumSignalBehavior
	Immediate: EnumSignalBehavior
	Deferred: EnumSignalBehavior
	AncestryDeferred: EnumSignalBehavior
end
declare class EnumSizeConstraint extends EnumItem end
declare class EnumSizeConstraint_INTERNAL extends Enum
	RelativeXY: EnumSizeConstraint
	RelativeXX: EnumSizeConstraint
	RelativeYY: EnumSizeConstraint
end
declare class EnumSolverConvergenceVisualizationMode extends EnumItem end
declare class EnumSolverConvergenceVisualizationMode_INTERNAL extends Enum
	Disabled: EnumSolverConvergenceVisualizationMode
	PerIsland: EnumSolverConvergenceVisualizationMode
	PerEdge: EnumSolverConvergenceVisualizationMode
end
declare class EnumSortDirection extends EnumItem end
declare class EnumSortDirection_INTERNAL extends Enum
	Ascending: EnumSortDirection
	Descending: EnumSortDirection
end
declare class EnumSortOrder extends EnumItem end
declare class EnumSortOrder_INTERNAL extends Enum
	LayoutOrder: EnumSortOrder
	Name: EnumSortOrder
	Custom: EnumSortOrder
end
declare class EnumSpecialKey extends EnumItem end
declare class EnumSpecialKey_INTERNAL extends Enum
	Insert: EnumSpecialKey
	Home: EnumSpecialKey
	End: EnumSpecialKey
	PageUp: EnumSpecialKey
	PageDown: EnumSpecialKey
	ChatHotkey: EnumSpecialKey
end
declare class EnumStartCorner extends EnumItem end
declare class EnumStartCorner_INTERNAL extends Enum
	TopLeft: EnumStartCorner
	TopRight: EnumStartCorner
	BottomLeft: EnumStartCorner
	BottomRight: EnumStartCorner
end
declare class EnumStatus extends EnumItem end
declare class EnumStatus_INTERNAL extends Enum
	Poison: EnumStatus
	Confusion: EnumStatus
end
declare class EnumStreamOutBehavior extends EnumItem end
declare class EnumStreamOutBehavior_INTERNAL extends Enum
	Default: EnumStreamOutBehavior
	LowMemory: EnumStreamOutBehavior
	Opportunistic: EnumStreamOutBehavior
end
declare class EnumStreamingIntegrityMode extends EnumItem end
declare class EnumStreamingIntegrityMode_INTERNAL extends Enum
	Default: EnumStreamingIntegrityMode
	Disabled: EnumStreamingIntegrityMode
	MinimumRadiusPause: EnumStreamingIntegrityMode
	PauseOutsideLoadedArea: EnumStreamingIntegrityMode
end
declare class EnumStreamingPauseMode extends EnumItem end
declare class EnumStreamingPauseMode_INTERNAL extends Enum
	Default: EnumStreamingPauseMode
	Disabled: EnumStreamingPauseMode
	ClientPhysicsPause: EnumStreamingPauseMode
end
declare class EnumStudioCloseMode extends EnumItem end
declare class EnumStudioCloseMode_INTERNAL extends Enum
	None: EnumStudioCloseMode
	CloseStudio: EnumStudioCloseMode
	CloseDoc: EnumStudioCloseMode
end
declare class EnumStudioDataModelType extends EnumItem end
declare class EnumStudioDataModelType_INTERNAL extends Enum
	Edit: EnumStudioDataModelType
	PlayClient: EnumStudioDataModelType
	PlayServer: EnumStudioDataModelType
	Standalone: EnumStudioDataModelType
	None: EnumStudioDataModelType
end
declare class EnumStudioScriptEditorColorCategories extends EnumItem end
declare class EnumStudioScriptEditorColorCategories_INTERNAL extends Enum
	Default: EnumStudioScriptEditorColorCategories
	Operator: EnumStudioScriptEditorColorCategories
	Number: EnumStudioScriptEditorColorCategories
	String: EnumStudioScriptEditorColorCategories
	Comment: EnumStudioScriptEditorColorCategories
	Keyword: EnumStudioScriptEditorColorCategories
	Builtin: EnumStudioScriptEditorColorCategories
	Method: EnumStudioScriptEditorColorCategories
	Property: EnumStudioScriptEditorColorCategories
	Nil: EnumStudioScriptEditorColorCategories
	Bool: EnumStudioScriptEditorColorCategories
	Function: EnumStudioScriptEditorColorCategories
	Local: EnumStudioScriptEditorColorCategories
	Self: EnumStudioScriptEditorColorCategories
	LuauKeyword: EnumStudioScriptEditorColorCategories
	FunctionName: EnumStudioScriptEditorColorCategories
	TODO: EnumStudioScriptEditorColorCategories
	Background: EnumStudioScriptEditorColorCategories
	SelectionText: EnumStudioScriptEditorColorCategories
	SelectionBackground: EnumStudioScriptEditorColorCategories
	FindSelectionBackground: EnumStudioScriptEditorColorCategories
	MatchingWordBackground: EnumStudioScriptEditorColorCategories
	Warning: EnumStudioScriptEditorColorCategories
	Error: EnumStudioScriptEditorColorCategories
	Info: EnumStudioScriptEditorColorCategories
	Hint: EnumStudioScriptEditorColorCategories
	Whitespace: EnumStudioScriptEditorColorCategories
	ActiveLine: EnumStudioScriptEditorColorCategories
	DebuggerCurrentLine: EnumStudioScriptEditorColorCategories
	DebuggerErrorLine: EnumStudioScriptEditorColorCategories
	Ruler: EnumStudioScriptEditorColorCategories
	Bracket: EnumStudioScriptEditorColorCategories
	MenuPrimaryText: EnumStudioScriptEditorColorCategories
	MenuSecondaryText: EnumStudioScriptEditorColorCategories
	MenuSelectedText: EnumStudioScriptEditorColorCategories
	MenuBackground: EnumStudioScriptEditorColorCategories
	MenuSelectedBackground: EnumStudioScriptEditorColorCategories
	MenuScrollbarBackground: EnumStudioScriptEditorColorCategories
	MenuScrollbarHandle: EnumStudioScriptEditorColorCategories
	MenuBorder: EnumStudioScriptEditorColorCategories
	DocViewCodeBackground: EnumStudioScriptEditorColorCategories
	AICOOverlayText: EnumStudioScriptEditorColorCategories
	AICOOverlayButtonBackground: EnumStudioScriptEditorColorCategories
	AICOOverlayButtonBackgroundHover: EnumStudioScriptEditorColorCategories
	AICOOverlayButtonBackgroundPressed: EnumStudioScriptEditorColorCategories
	IndentationRuler: EnumStudioScriptEditorColorCategories
end
declare class EnumStudioScriptEditorColorPresets extends EnumItem end
declare class EnumStudioScriptEditorColorPresets_INTERNAL extends Enum
	RobloxDefault: EnumStudioScriptEditorColorPresets
	Extra1: EnumStudioScriptEditorColorPresets
	Extra2: EnumStudioScriptEditorColorPresets
	Custom: EnumStudioScriptEditorColorPresets
end
declare class EnumStudioStyleGuideColor extends EnumItem end
declare class EnumStudioStyleGuideColor_INTERNAL extends Enum
	MainBackground: EnumStudioStyleGuideColor
	Titlebar: EnumStudioStyleGuideColor
	Dropdown: EnumStudioStyleGuideColor
	Tooltip: EnumStudioStyleGuideColor
	Notification: EnumStudioStyleGuideColor
	ScrollBar: EnumStudioStyleGuideColor
	ScrollBarBackground: EnumStudioStyleGuideColor
	TabBar: EnumStudioStyleGuideColor
	Tab: EnumStudioStyleGuideColor
	FilterButtonDefault: EnumStudioStyleGuideColor
	FilterButtonHover: EnumStudioStyleGuideColor
	FilterButtonChecked: EnumStudioStyleGuideColor
	FilterButtonAccent: EnumStudioStyleGuideColor
	FilterButtonBorder: EnumStudioStyleGuideColor
	FilterButtonBorderAlt: EnumStudioStyleGuideColor
	RibbonTab: EnumStudioStyleGuideColor
	RibbonTabTopBar: EnumStudioStyleGuideColor
	Button: EnumStudioStyleGuideColor
	MainButton: EnumStudioStyleGuideColor
	RibbonButton: EnumStudioStyleGuideColor
	ViewPortBackground: EnumStudioStyleGuideColor
	InputFieldBackground: EnumStudioStyleGuideColor
	Item: EnumStudioStyleGuideColor
	TableItem: EnumStudioStyleGuideColor
	CategoryItem: EnumStudioStyleGuideColor
	GameSettingsTableItem: EnumStudioStyleGuideColor
	GameSettingsTooltip: EnumStudioStyleGuideColor
	EmulatorBar: EnumStudioStyleGuideColor
	EmulatorDropDown: EnumStudioStyleGuideColor
	ColorPickerFrame: EnumStudioStyleGuideColor
	CurrentMarker: EnumStudioStyleGuideColor
	Border: EnumStudioStyleGuideColor
	DropShadow: EnumStudioStyleGuideColor
	Shadow: EnumStudioStyleGuideColor
	Light: EnumStudioStyleGuideColor
	Dark: EnumStudioStyleGuideColor
	Mid: EnumStudioStyleGuideColor
	MainText: EnumStudioStyleGuideColor
	SubText: EnumStudioStyleGuideColor
	TitlebarText: EnumStudioStyleGuideColor
	BrightText: EnumStudioStyleGuideColor
	DimmedText: EnumStudioStyleGuideColor
	LinkText: EnumStudioStyleGuideColor
	WarningText: EnumStudioStyleGuideColor
	ErrorText: EnumStudioStyleGuideColor
	InfoText: EnumStudioStyleGuideColor
	SensitiveText: EnumStudioStyleGuideColor
	ScriptSideWidget: EnumStudioStyleGuideColor
	ScriptBackground: EnumStudioStyleGuideColor
	ScriptText: EnumStudioStyleGuideColor
	ScriptSelectionText: EnumStudioStyleGuideColor
	ScriptSelectionBackground: EnumStudioStyleGuideColor
	ScriptFindSelectionBackground: EnumStudioStyleGuideColor
	ScriptMatchingWordSelectionBackground: EnumStudioStyleGuideColor
	ScriptOperator: EnumStudioStyleGuideColor
	ScriptNumber: EnumStudioStyleGuideColor
	ScriptString: EnumStudioStyleGuideColor
	ScriptComment: EnumStudioStyleGuideColor
	ScriptKeyword: EnumStudioStyleGuideColor
	ScriptBuiltInFunction: EnumStudioStyleGuideColor
	ScriptWarning: EnumStudioStyleGuideColor
	ScriptError: EnumStudioStyleGuideColor
	ScriptInformation: EnumStudioStyleGuideColor
	ScriptHint: EnumStudioStyleGuideColor
	ScriptWhitespace: EnumStudioStyleGuideColor
	ScriptRuler: EnumStudioStyleGuideColor
	DocViewCodeBackground: EnumStudioStyleGuideColor
	DebuggerCurrentLine: EnumStudioStyleGuideColor
	DebuggerErrorLine: EnumStudioStyleGuideColor
	ScriptEditorCurrentLine: EnumStudioStyleGuideColor
	DiffFilePathText: EnumStudioStyleGuideColor
	DiffTextHunkInfo: EnumStudioStyleGuideColor
	DiffTextNoChange: EnumStudioStyleGuideColor
	DiffTextAddition: EnumStudioStyleGuideColor
	DiffTextDeletion: EnumStudioStyleGuideColor
	DiffTextSeparatorBackground: EnumStudioStyleGuideColor
	DiffTextNoChangeBackground: EnumStudioStyleGuideColor
	DiffTextAdditionBackground: EnumStudioStyleGuideColor
	DiffTextDeletionBackground: EnumStudioStyleGuideColor
	DiffLineNum: EnumStudioStyleGuideColor
	DiffLineNumSeparatorBackground: EnumStudioStyleGuideColor
	DiffLineNumNoChangeBackground: EnumStudioStyleGuideColor
	DiffLineNumAdditionBackground: EnumStudioStyleGuideColor
	DiffLineNumDeletionBackground: EnumStudioStyleGuideColor
	DiffFilePathBackground: EnumStudioStyleGuideColor
	DiffFilePathBorder: EnumStudioStyleGuideColor
	ChatIncomingBgColor: EnumStudioStyleGuideColor
	ChatIncomingTextColor: EnumStudioStyleGuideColor
	ChatOutgoingBgColor: EnumStudioStyleGuideColor
	ChatOutgoingTextColor: EnumStudioStyleGuideColor
	ChatModeratedMessageColor: EnumStudioStyleGuideColor
	Separator: EnumStudioStyleGuideColor
	ButtonBorder: EnumStudioStyleGuideColor
	ButtonText: EnumStudioStyleGuideColor
	InputFieldBorder: EnumStudioStyleGuideColor
	CheckedFieldBackground: EnumStudioStyleGuideColor
	CheckedFieldBorder: EnumStudioStyleGuideColor
	CheckedFieldIndicator: EnumStudioStyleGuideColor
	HeaderSection: EnumStudioStyleGuideColor
	Midlight: EnumStudioStyleGuideColor
	StatusBar: EnumStudioStyleGuideColor
	DialogButton: EnumStudioStyleGuideColor
	DialogButtonText: EnumStudioStyleGuideColor
	DialogButtonBorder: EnumStudioStyleGuideColor
	DialogMainButton: EnumStudioStyleGuideColor
	DialogMainButtonText: EnumStudioStyleGuideColor
	InfoBarWarningBackground: EnumStudioStyleGuideColor
	InfoBarWarningText: EnumStudioStyleGuideColor
	ScriptMethod: EnumStudioStyleGuideColor
	ScriptProperty: EnumStudioStyleGuideColor
	ScriptNil: EnumStudioStyleGuideColor
	ScriptBool: EnumStudioStyleGuideColor
	ScriptFunction: EnumStudioStyleGuideColor
	ScriptLocal: EnumStudioStyleGuideColor
	ScriptSelf: EnumStudioStyleGuideColor
	ScriptLuauKeyword: EnumStudioStyleGuideColor
	ScriptFunctionName: EnumStudioStyleGuideColor
	ScriptTodo: EnumStudioStyleGuideColor
	ScriptBracket: EnumStudioStyleGuideColor
	AICOOverlayText: EnumStudioStyleGuideColor
	AICOOverlayButtonBackground: EnumStudioStyleGuideColor
	AICOOverlayButtonBackgroundHover: EnumStudioStyleGuideColor
	AICOOverlayButtonBackgroundPressed: EnumStudioStyleGuideColor
	AttributeCog: EnumStudioStyleGuideColor
end
declare class EnumStudioStyleGuideModifier extends EnumItem end
declare class EnumStudioStyleGuideModifier_INTERNAL extends Enum
	Default: EnumStudioStyleGuideModifier
	Selected: EnumStudioStyleGuideModifier
	Pressed: EnumStudioStyleGuideModifier
	Disabled: EnumStudioStyleGuideModifier
	Hover: EnumStudioStyleGuideModifier
end
declare class EnumStyle extends EnumItem end
declare class EnumStyle_INTERNAL extends Enum
	AlternatingSupports: EnumStyle
	BridgeStyleSupports: EnumStyle
	NoSupports: EnumStyle
end
declare class EnumSurfaceConstraint extends EnumItem end
declare class EnumSurfaceConstraint_INTERNAL extends Enum
	None: EnumSurfaceConstraint
	Hinge: EnumSurfaceConstraint
	SteppingMotor: EnumSurfaceConstraint
	Motor: EnumSurfaceConstraint
end
declare class EnumSurfaceGuiShape extends EnumItem end
declare class EnumSurfaceGuiShape_INTERNAL extends Enum
	Flat: EnumSurfaceGuiShape
	CurvedHorizontally: EnumSurfaceGuiShape
end
declare class EnumSurfaceGuiSizingMode extends EnumItem end
declare class EnumSurfaceGuiSizingMode_INTERNAL extends Enum
	FixedSize: EnumSurfaceGuiSizingMode
	PixelsPerStud: EnumSurfaceGuiSizingMode
end
declare class EnumSurfaceType extends EnumItem end
declare class EnumSurfaceType_INTERNAL extends Enum
	Smooth: EnumSurfaceType
	Glue: EnumSurfaceType
	Weld: EnumSurfaceType
	Studs: EnumSurfaceType
	Inlet: EnumSurfaceType
	Universal: EnumSurfaceType
	Hinge: EnumSurfaceType
	Motor: EnumSurfaceType
	SteppingMotor: EnumSurfaceType
	SmoothNoOutlines: EnumSurfaceType
end
declare class EnumSwipeDirection extends EnumItem end
declare class EnumSwipeDirection_INTERNAL extends Enum
	Right: EnumSwipeDirection
	Left: EnumSwipeDirection
	Up: EnumSwipeDirection
	Down: EnumSwipeDirection
	None: EnumSwipeDirection
end
declare class EnumTableMajorAxis extends EnumItem end
declare class EnumTableMajorAxis_INTERNAL extends Enum
	RowMajor: EnumTableMajorAxis
	ColumnMajor: EnumTableMajorAxis
end
declare class EnumTechnology extends EnumItem end
declare class EnumTechnology_INTERNAL extends Enum
	Compatibility: EnumTechnology
	Voxel: EnumTechnology
	ShadowMap: EnumTechnology
	Legacy: EnumTechnology
	Future: EnumTechnology
end
declare class EnumTeleportMethod extends EnumItem end
declare class EnumTeleportMethod_INTERNAL extends Enum
	TeleportToSpawnByName: EnumTeleportMethod
	TeleportToPlaceInstance: EnumTeleportMethod
	TeleportToPrivateServer: EnumTeleportMethod
	TeleportPartyAsync: EnumTeleportMethod
	TeleportUnknown: EnumTeleportMethod
end
declare class EnumTeleportResult extends EnumItem end
declare class EnumTeleportResult_INTERNAL extends Enum
	Success: EnumTeleportResult
	Failure: EnumTeleportResult
	GameNotFound: EnumTeleportResult
	GameEnded: EnumTeleportResult
	GameFull: EnumTeleportResult
	Unauthorized: EnumTeleportResult
	Flooded: EnumTeleportResult
	IsTeleporting: EnumTeleportResult
end
declare class EnumTeleportState extends EnumItem end
declare class EnumTeleportState_INTERNAL extends Enum
	RequestedFromServer: EnumTeleportState
	Started: EnumTeleportState
	WaitingForServer: EnumTeleportState
	Failed: EnumTeleportState
	InProgress: EnumTeleportState
end
declare class EnumTeleportType extends EnumItem end
declare class EnumTeleportType_INTERNAL extends Enum
	ToPlace: EnumTeleportType
	ToInstance: EnumTeleportType
	ToReservedServer: EnumTeleportType
end
declare class EnumTerrainAcquisitionMethod extends EnumItem end
declare class EnumTerrainAcquisitionMethod_INTERNAL extends Enum
	None: EnumTerrainAcquisitionMethod
	Legacy: EnumTerrainAcquisitionMethod
	Template: EnumTerrainAcquisitionMethod
	Generate: EnumTerrainAcquisitionMethod
	Import: EnumTerrainAcquisitionMethod
	Convert: EnumTerrainAcquisitionMethod
	EditAddTool: EnumTerrainAcquisitionMethod
	EditSeaLevelTool: EnumTerrainAcquisitionMethod
	EditReplaceTool: EnumTerrainAcquisitionMethod
	RegionFillTool: EnumTerrainAcquisitionMethod
	RegionPasteTool: EnumTerrainAcquisitionMethod
	Other: EnumTerrainAcquisitionMethod
end
declare class EnumTerrainFace extends EnumItem end
declare class EnumTerrainFace_INTERNAL extends Enum
	Top: EnumTerrainFace
	Side: EnumTerrainFace
	Bottom: EnumTerrainFace
end
declare class EnumTextChatMessageStatus extends EnumItem end
declare class EnumTextChatMessageStatus_INTERNAL extends Enum
	Unknown: EnumTextChatMessageStatus
	Success: EnumTextChatMessageStatus
	Sending: EnumTextChatMessageStatus
	TextFilterFailed: EnumTextChatMessageStatus
	Floodchecked: EnumTextChatMessageStatus
	InvalidPrivacySettings: EnumTextChatMessageStatus
	InvalidTextChannelPermissions: EnumTextChatMessageStatus
	MessageTooLong: EnumTextChatMessageStatus
end
declare class EnumTextDirection extends EnumItem end
declare class EnumTextDirection_INTERNAL extends Enum
	Auto: EnumTextDirection
	LeftToRight: EnumTextDirection
	RightToLeft: EnumTextDirection
end
declare class EnumTextFilterContext extends EnumItem end
declare class EnumTextFilterContext_INTERNAL extends Enum
	PublicChat: EnumTextFilterContext
	PrivateChat: EnumTextFilterContext
end
declare class EnumTextInputType extends EnumItem end
declare class EnumTextInputType_INTERNAL extends Enum
	Default: EnumTextInputType
	NoSuggestions: EnumTextInputType
	Number: EnumTextInputType
	Email: EnumTextInputType
	Phone: EnumTextInputType
	Password: EnumTextInputType
	PasswordShown: EnumTextInputType
	Username: EnumTextInputType
	OneTimePassword: EnumTextInputType
end
declare class EnumTextTruncate extends EnumItem end
declare class EnumTextTruncate_INTERNAL extends Enum
	None: EnumTextTruncate
	AtEnd: EnumTextTruncate
end
declare class EnumTextXAlignment extends EnumItem end
declare class EnumTextXAlignment_INTERNAL extends Enum
	Left: EnumTextXAlignment
	Center: EnumTextXAlignment
	Right: EnumTextXAlignment
end
declare class EnumTextYAlignment extends EnumItem end
declare class EnumTextYAlignment_INTERNAL extends Enum
	Top: EnumTextYAlignment
	Center: EnumTextYAlignment
	Bottom: EnumTextYAlignment
end
declare class EnumTextureMode extends EnumItem end
declare class EnumTextureMode_INTERNAL extends Enum
	Stretch: EnumTextureMode
	Wrap: EnumTextureMode
	Static: EnumTextureMode
end
declare class EnumTextureQueryType extends EnumItem end
declare class EnumTextureQueryType_INTERNAL extends Enum
	NonHumanoid: EnumTextureQueryType
	NonHumanoidOrphaned: EnumTextureQueryType
	Humanoid: EnumTextureQueryType
	HumanoidOrphaned: EnumTextureQueryType
end
declare class EnumThreadPoolConfig extends EnumItem end
declare class EnumThreadPoolConfig_INTERNAL extends Enum
	Auto: EnumThreadPoolConfig
	PerCore1: EnumThreadPoolConfig
	PerCore2: EnumThreadPoolConfig
	PerCore3: EnumThreadPoolConfig
	PerCore4: EnumThreadPoolConfig
	Threads1: EnumThreadPoolConfig
	Threads2: EnumThreadPoolConfig
	Threads3: EnumThreadPoolConfig
	Threads4: EnumThreadPoolConfig
	Threads8: EnumThreadPoolConfig
	Threads16: EnumThreadPoolConfig
end
declare class EnumThrottlingPriority extends EnumItem end
declare class EnumThrottlingPriority_INTERNAL extends Enum
	Extreme: EnumThrottlingPriority
	ElevatedOnServer: EnumThrottlingPriority
	Default: EnumThrottlingPriority
end
declare class EnumThumbnailSize extends EnumItem end
declare class EnumThumbnailSize_INTERNAL extends Enum
	Size48x48: EnumThumbnailSize
	Size180x180: EnumThumbnailSize
	Size420x420: EnumThumbnailSize
	Size60x60: EnumThumbnailSize
	Size100x100: EnumThumbnailSize
	Size150x150: EnumThumbnailSize
	Size352x352: EnumThumbnailSize
end
declare class EnumThumbnailType extends EnumItem end
declare class EnumThumbnailType_INTERNAL extends Enum
	HeadShot: EnumThumbnailType
	AvatarBust: EnumThumbnailType
	AvatarThumbnail: EnumThumbnailType
end
declare class EnumTickCountSampleMethod extends EnumItem end
declare class EnumTickCountSampleMethod_INTERNAL extends Enum
	Fast: EnumTickCountSampleMethod
	Benchmark: EnumTickCountSampleMethod
	Precise: EnumTickCountSampleMethod
end
declare class EnumTopBottom extends EnumItem end
declare class EnumTopBottom_INTERNAL extends Enum
	Top: EnumTopBottom
	Center: EnumTopBottom
	Bottom: EnumTopBottom
end
declare class EnumTouchCameraMovementMode extends EnumItem end
declare class EnumTouchCameraMovementMode_INTERNAL extends Enum
	Default: EnumTouchCameraMovementMode
	Follow: EnumTouchCameraMovementMode
	Classic: EnumTouchCameraMovementMode
	Orbital: EnumTouchCameraMovementMode
end
declare class EnumTouchMovementMode extends EnumItem end
declare class EnumTouchMovementMode_INTERNAL extends Enum
	Default: EnumTouchMovementMode
	Thumbstick: EnumTouchMovementMode
	DPad: EnumTouchMovementMode
	Thumbpad: EnumTouchMovementMode
	ClickToMove: EnumTouchMovementMode
	DynamicThumbstick: EnumTouchMovementMode
end
declare class EnumTrackerError extends EnumItem end
declare class EnumTrackerError_INTERNAL extends Enum
	Ok: EnumTrackerError
	NoService: EnumTrackerError
	InitFailed: EnumTrackerError
	NoVideo: EnumTrackerError
	VideoError: EnumTrackerError
	VideoNoPermission: EnumTrackerError
	VideoUnsupported: EnumTrackerError
	NoAudio: EnumTrackerError
	AudioError: EnumTrackerError
	AudioNoPermission: EnumTrackerError
	UnsupportedDevice: EnumTrackerError
end
declare class EnumTrackerExtrapolationFlagMode extends EnumItem end
declare class EnumTrackerExtrapolationFlagMode_INTERNAL extends Enum
	Auto: EnumTrackerExtrapolationFlagMode
	ForceDisabled: EnumTrackerExtrapolationFlagMode
	ExtrapolateFacsAndPose: EnumTrackerExtrapolationFlagMode
	ExtrapolateFacsOnly: EnumTrackerExtrapolationFlagMode
end
declare class EnumTrackerLodFlagMode extends EnumItem end
declare class EnumTrackerLodFlagMode_INTERNAL extends Enum
	Auto: EnumTrackerLodFlagMode
	ForceFalse: EnumTrackerLodFlagMode
	ForceTrue: EnumTrackerLodFlagMode
end
declare class EnumTrackerLodValueMode extends EnumItem end
declare class EnumTrackerLodValueMode_INTERNAL extends Enum
	Auto: EnumTrackerLodValueMode
	Force0: EnumTrackerLodValueMode
	Force1: EnumTrackerLodValueMode
end
declare class EnumTrackerMode extends EnumItem end
declare class EnumTrackerMode_INTERNAL extends Enum
	None: EnumTrackerMode
	Audio: EnumTrackerMode
	Video: EnumTrackerMode
	AudioVideo: EnumTrackerMode
end
declare class EnumTrackerPromptEvent extends EnumItem end
declare class EnumTrackerPromptEvent_INTERNAL extends Enum
	LODCameraRecommendDisable: EnumTrackerPromptEvent
end
declare class EnumTriStateBoolean extends EnumItem end
declare class EnumTriStateBoolean_INTERNAL extends Enum
	Unknown: EnumTriStateBoolean
	True: EnumTriStateBoolean
	False: EnumTriStateBoolean
end
declare class EnumTweenStatus extends EnumItem end
declare class EnumTweenStatus_INTERNAL extends Enum
	Canceled: EnumTweenStatus
	Completed: EnumTweenStatus
end
declare class EnumUITheme extends EnumItem end
declare class EnumUITheme_INTERNAL extends Enum
	Light: EnumUITheme
	Dark: EnumUITheme
end
declare class EnumUiMessageType extends EnumItem end
declare class EnumUiMessageType_INTERNAL extends Enum
	UiMessageError: EnumUiMessageType
	UiMessageInfo: EnumUiMessageType
end
declare class EnumUsageContext extends EnumItem end
declare class EnumUsageContext_INTERNAL extends Enum
	Default: EnumUsageContext
	Preview: EnumUsageContext
end
declare class EnumUserCFrame extends EnumItem end
declare class EnumUserCFrame_INTERNAL extends Enum
	Head: EnumUserCFrame
	LeftHand: EnumUserCFrame
	RightHand: EnumUserCFrame
	Floor: EnumUserCFrame
end
declare class EnumUserInputState extends EnumItem end
declare class EnumUserInputState_INTERNAL extends Enum
	Begin: EnumUserInputState
	Change: EnumUserInputState
	End: EnumUserInputState
	Cancel: EnumUserInputState
	None: EnumUserInputState
end
declare class EnumUserInputType extends EnumItem end
declare class EnumUserInputType_INTERNAL extends Enum
	MouseButton1: EnumUserInputType
	MouseButton2: EnumUserInputType
	MouseButton3: EnumUserInputType
	MouseWheel: EnumUserInputType
	MouseMovement: EnumUserInputType
	Touch: EnumUserInputType
	Keyboard: EnumUserInputType
	Focus: EnumUserInputType
	Accelerometer: EnumUserInputType
	Gyro: EnumUserInputType
	Gamepad1: EnumUserInputType
	Gamepad2: EnumUserInputType
	Gamepad3: EnumUserInputType
	Gamepad4: EnumUserInputType
	Gamepad5: EnumUserInputType
	Gamepad6: EnumUserInputType
	Gamepad7: EnumUserInputType
	Gamepad8: EnumUserInputType
	TextInput: EnumUserInputType
	InputMethod: EnumUserInputType
	None: EnumUserInputType
end
declare class EnumVRSafetyBubbleMode extends EnumItem end
declare class EnumVRSafetyBubbleMode_INTERNAL extends Enum
	NoOne: EnumVRSafetyBubbleMode
	OnlyFriends: EnumVRSafetyBubbleMode
	Anyone: EnumVRSafetyBubbleMode
end
declare class EnumVRScaling extends EnumItem end
declare class EnumVRScaling_INTERNAL extends Enum
	World: EnumVRScaling
	Off: EnumVRScaling
end
declare class EnumVRSessionState extends EnumItem end
declare class EnumVRSessionState_INTERNAL extends Enum
	Idle: EnumVRSessionState
	Visible: EnumVRSessionState
	Focused: EnumVRSessionState
	Stopping: EnumVRSessionState
	Undefined: EnumVRSessionState
end
declare class EnumVRTouchpad extends EnumItem end
declare class EnumVRTouchpad_INTERNAL extends Enum
	Left: EnumVRTouchpad
	Right: EnumVRTouchpad
end
declare class EnumVRTouchpadMode extends EnumItem end
declare class EnumVRTouchpadMode_INTERNAL extends Enum
	Touch: EnumVRTouchpadMode
	VirtualThumbstick: EnumVRTouchpadMode
	ABXY: EnumVRTouchpadMode
end
declare class EnumVelocityConstraintMode extends EnumItem end
declare class EnumVelocityConstraintMode_INTERNAL extends Enum
	Line: EnumVelocityConstraintMode
	Plane: EnumVelocityConstraintMode
	Vector: EnumVelocityConstraintMode
end
declare class EnumVerticalAlignment extends EnumItem end
declare class EnumVerticalAlignment_INTERNAL extends Enum
	Center: EnumVerticalAlignment
	Top: EnumVerticalAlignment
	Bottom: EnumVerticalAlignment
end
declare class EnumVerticalScrollBarPosition extends EnumItem end
declare class EnumVerticalScrollBarPosition_INTERNAL extends Enum
	Left: EnumVerticalScrollBarPosition
	Right: EnumVerticalScrollBarPosition
end
declare class EnumVibrationMotor extends EnumItem end
declare class EnumVibrationMotor_INTERNAL extends Enum
	Large: EnumVibrationMotor
	Small: EnumVibrationMotor
	LeftTrigger: EnumVibrationMotor
	RightTrigger: EnumVibrationMotor
	LeftHand: EnumVibrationMotor
	RightHand: EnumVibrationMotor
end
declare class EnumViewMode extends EnumItem end
declare class EnumViewMode_INTERNAL extends Enum
	None: EnumViewMode
	GeometryComplexity: EnumViewMode
	Transparent: EnumViewMode
	Decal: EnumViewMode
end
declare class EnumVirtualCursorMode extends EnumItem end
declare class EnumVirtualCursorMode_INTERNAL extends Enum
	Default: EnumVirtualCursorMode
	Disabled: EnumVirtualCursorMode
	Enabled: EnumVirtualCursorMode
end
declare class EnumVirtualInputMode extends EnumItem end
declare class EnumVirtualInputMode_INTERNAL extends Enum
	Recording: EnumVirtualInputMode
	Playing: EnumVirtualInputMode
	None: EnumVirtualInputMode
end
declare class EnumVoiceChatState extends EnumItem end
declare class EnumVoiceChatState_INTERNAL extends Enum
	Idle: EnumVoiceChatState
	Joining: EnumVoiceChatState
	JoiningRetry: EnumVoiceChatState
	Joined: EnumVoiceChatState
	Leaving: EnumVoiceChatState
	Ended: EnumVoiceChatState
	Failed: EnumVoiceChatState
end
declare class EnumVolumetricAudio extends EnumItem end
declare class EnumVolumetricAudio_INTERNAL extends Enum
	Disabled: EnumVolumetricAudio
	Automatic: EnumVolumetricAudio
	Enabled: EnumVolumetricAudio
end
declare class EnumWaterDirection extends EnumItem end
declare class EnumWaterDirection_INTERNAL extends Enum
	NegX: EnumWaterDirection
	X: EnumWaterDirection
	NegY: EnumWaterDirection
	Y: EnumWaterDirection
	NegZ: EnumWaterDirection
	Z: EnumWaterDirection
end
declare class EnumWaterForce extends EnumItem end
declare class EnumWaterForce_INTERNAL extends Enum
	None: EnumWaterForce
	Small: EnumWaterForce
	Medium: EnumWaterForce
	Strong: EnumWaterForce
	Max: EnumWaterForce
end
declare class EnumWeldConstraintPreserve extends EnumItem end
declare class EnumWeldConstraintPreserve_INTERNAL extends Enum
	All: EnumWeldConstraintPreserve
	None: EnumWeldConstraintPreserve
	Touching: EnumWeldConstraintPreserve
end
declare class EnumWrapLayerAutoSkin extends EnumItem end
declare class EnumWrapLayerAutoSkin_INTERNAL extends Enum
	Disabled: EnumWrapLayerAutoSkin
	EnabledPreserve: EnumWrapLayerAutoSkin
	EnabledOverride: EnumWrapLayerAutoSkin
end
declare class EnumWrapLayerDebugMode extends EnumItem end
declare class EnumWrapLayerDebugMode_INTERNAL extends Enum
	None: EnumWrapLayerDebugMode
	BoundCage: EnumWrapLayerDebugMode
	LayerCage: EnumWrapLayerDebugMode
	BoundCageAndLinks: EnumWrapLayerDebugMode
	Reference: EnumWrapLayerDebugMode
	Rbf: EnumWrapLayerDebugMode
	OuterCage: EnumWrapLayerDebugMode
	ReferenceMeshAfterMorph: EnumWrapLayerDebugMode
	HSROuterDetail: EnumWrapLayerDebugMode
	HSROuter: EnumWrapLayerDebugMode
	HSRInner: EnumWrapLayerDebugMode
	HSRInnerReverse: EnumWrapLayerDebugMode
	LayerCageFittedToBase: EnumWrapLayerDebugMode
	LayerCageFittedToPrev: EnumWrapLayerDebugMode
end
declare class EnumWrapTargetDebugMode extends EnumItem end
declare class EnumWrapTargetDebugMode_INTERNAL extends Enum
	None: EnumWrapTargetDebugMode
	TargetCageOriginal: EnumWrapTargetDebugMode
	TargetCageCompressed: EnumWrapTargetDebugMode
	TargetCageInterface: EnumWrapTargetDebugMode
	TargetLayerCageOriginal: EnumWrapTargetDebugMode
	TargetLayerCageCompressed: EnumWrapTargetDebugMode
	TargetLayerInterface: EnumWrapTargetDebugMode
	Rbf: EnumWrapTargetDebugMode
	OuterCageDetail: EnumWrapTargetDebugMode
end
declare class EnumZIndexBehavior extends EnumItem end
declare class EnumZIndexBehavior_INTERNAL extends Enum
	Global: EnumZIndexBehavior
	Sibling: EnumZIndexBehavior
end


type ENUM_LIST = {
	AccessoryType: EnumAccessoryType_INTERNAL,
	ActionType: EnumActionType_INTERNAL,
	ActuatorRelativeTo: EnumActuatorRelativeTo_INTERNAL,
	ActuatorType: EnumActuatorType_INTERNAL,
	AdShape: EnumAdShape_INTERNAL,
	AdTeleportMethod: EnumAdTeleportMethod_INTERNAL,
	AdUnitStatus: EnumAdUnitStatus_INTERNAL,
	AdornCullingMode: EnumAdornCullingMode_INTERNAL,
	AlignType: EnumAlignType_INTERNAL,
	AlphaMode: EnumAlphaMode_INTERNAL,
	AnalyticsEconomyAction: EnumAnalyticsEconomyAction_INTERNAL,
	AnalyticsLogLevel: EnumAnalyticsLogLevel_INTERNAL,
	AnalyticsProgressionStatus: EnumAnalyticsProgressionStatus_INTERNAL,
	AnimationPriority: EnumAnimationPriority_INTERNAL,
	AnimatorRetargetingMode: EnumAnimatorRetargetingMode_INTERNAL,
	AppShellActionType: EnumAppShellActionType_INTERNAL,
	AppShellFeature: EnumAppShellFeature_INTERNAL,
	AppUpdateStatus: EnumAppUpdateStatus_INTERNAL,
	ApplyStrokeMode: EnumApplyStrokeMode_INTERNAL,
	AspectType: EnumAspectType_INTERNAL,
	AssetFetchStatus: EnumAssetFetchStatus_INTERNAL,
	AssetType: EnumAssetType_INTERNAL,
	AssetTypeVerification: EnumAssetTypeVerification_INTERNAL,
	AudioSubType: EnumAudioSubType_INTERNAL,
	AutoIndentRule: EnumAutoIndentRule_INTERNAL,
	AutomaticSize: EnumAutomaticSize_INTERNAL,
	AvatarAssetType: EnumAvatarAssetType_INTERNAL,
	AvatarChatServiceFeature: EnumAvatarChatServiceFeature_INTERNAL,
	AvatarContextMenuOption: EnumAvatarContextMenuOption_INTERNAL,
	AvatarItemType: EnumAvatarItemType_INTERNAL,
	AvatarJointUpgrade: EnumAvatarJointUpgrade_INTERNAL,
	AvatarPromptResult: EnumAvatarPromptResult_INTERNAL,
	AvatarThumbnailCustomizationType: EnumAvatarThumbnailCustomizationType_INTERNAL,
	AvatarUnificationMode: EnumAvatarUnificationMode_INTERNAL,
	Axis: EnumAxis_INTERNAL,
	BinType: EnumBinType_INTERNAL,
	BodyPart: EnumBodyPart_INTERNAL,
	BodyPartR15: EnumBodyPartR15_INTERNAL,
	BorderMode: EnumBorderMode_INTERNAL,
	BreakReason: EnumBreakReason_INTERNAL,
	BreakpointRemoveReason: EnumBreakpointRemoveReason_INTERNAL,
	BulkMoveMode: EnumBulkMoveMode_INTERNAL,
	BundleType: EnumBundleType_INTERNAL,
	Button: EnumButton_INTERNAL,
	ButtonStyle: EnumButtonStyle_INTERNAL,
	CageType: EnumCageType_INTERNAL,
	CameraMode: EnumCameraMode_INTERNAL,
	CameraPanMode: EnumCameraPanMode_INTERNAL,
	CameraType: EnumCameraType_INTERNAL,
	CatalogCategoryFilter: EnumCatalogCategoryFilter_INTERNAL,
	CatalogSortAggregation: EnumCatalogSortAggregation_INTERNAL,
	CatalogSortType: EnumCatalogSortType_INTERNAL,
	CellBlock: EnumCellBlock_INTERNAL,
	CellMaterial: EnumCellMaterial_INTERNAL,
	CellOrientation: EnumCellOrientation_INTERNAL,
	CenterDialogType: EnumCenterDialogType_INTERNAL,
	ChatCallbackType: EnumChatCallbackType_INTERNAL,
	ChatColor: EnumChatColor_INTERNAL,
	ChatMode: EnumChatMode_INTERNAL,
	ChatPrivacyMode: EnumChatPrivacyMode_INTERNAL,
	ChatStyle: EnumChatStyle_INTERNAL,
	ChatVersion: EnumChatVersion_INTERNAL,
	ClientAnimatorThrottlingMode: EnumClientAnimatorThrottlingMode_INTERNAL,
	CollisionFidelity: EnumCollisionFidelity_INTERNAL,
	CommandPermission: EnumCommandPermission_INTERNAL,
	CompileTarget: EnumCompileTarget_INTERNAL,
	CompletionItemKind: EnumCompletionItemKind_INTERNAL,
	CompletionItemTag: EnumCompletionItemTag_INTERNAL,
	CompletionTriggerKind: EnumCompletionTriggerKind_INTERNAL,
	ComputerCameraMovementMode: EnumComputerCameraMovementMode_INTERNAL,
	ComputerMovementMode: EnumComputerMovementMode_INTERNAL,
	ConnectionError: EnumConnectionError_INTERNAL,
	ConnectionState: EnumConnectionState_INTERNAL,
	ContextActionPriority: EnumContextActionPriority_INTERNAL,
	ContextActionResult: EnumContextActionResult_INTERNAL,
	ControlMode: EnumControlMode_INTERNAL,
	CoreGuiType: EnumCoreGuiType_INTERNAL,
	CreateOutfitFailure: EnumCreateOutfitFailure_INTERNAL,
	CreatorType: EnumCreatorType_INTERNAL,
	CreatorTypeFilter: EnumCreatorTypeFilter_INTERNAL,
	CurrencyType: EnumCurrencyType_INTERNAL,
	CustomCameraMode: EnumCustomCameraMode_INTERNAL,
	DataStoreRequestType: EnumDataStoreRequestType_INTERNAL,
	DeathStyle: EnumDeathStyle_INTERNAL,
	DebuggerEndReason: EnumDebuggerEndReason_INTERNAL,
	DebuggerExceptionBreakMode: EnumDebuggerExceptionBreakMode_INTERNAL,
	DebuggerFrameType: EnumDebuggerFrameType_INTERNAL,
	DebuggerPauseReason: EnumDebuggerPauseReason_INTERNAL,
	DebuggerStatus: EnumDebuggerStatus_INTERNAL,
	DevCameraOcclusionMode: EnumDevCameraOcclusionMode_INTERNAL,
	DevComputerCameraMovementMode: EnumDevComputerCameraMovementMode_INTERNAL,
	DevComputerMovementMode: EnumDevComputerMovementMode_INTERNAL,
	DevTouchCameraMovementMode: EnumDevTouchCameraMovementMode_INTERNAL,
	DevTouchMovementMode: EnumDevTouchMovementMode_INTERNAL,
	DeveloperMemoryTag: EnumDeveloperMemoryTag_INTERNAL,
	DeviceType: EnumDeviceType_INTERNAL,
	DialogBehaviorType: EnumDialogBehaviorType_INTERNAL,
	DialogPurpose: EnumDialogPurpose_INTERNAL,
	DialogTone: EnumDialogTone_INTERNAL,
	DominantAxis: EnumDominantAxis_INTERNAL,
	DraftStatusCode: EnumDraftStatusCode_INTERNAL,
	DragDetectorDragStyle: EnumDragDetectorDragStyle_INTERNAL,
	DragDetectorResponseStyle: EnumDragDetectorResponseStyle_INTERNAL,
	DraggerCoordinateSpace: EnumDraggerCoordinateSpace_INTERNAL,
	DraggerMovementMode: EnumDraggerMovementMode_INTERNAL,
	EasingDirection: EnumEasingDirection_INTERNAL,
	EasingStyle: EnumEasingStyle_INTERNAL,
	ElasticBehavior: EnumElasticBehavior_INTERNAL,
	EnviromentalPhysicsThrottle: EnumEnviromentalPhysicsThrottle_INTERNAL,
	ExperienceAuthScope: EnumExperienceAuthScope_INTERNAL,
	ExplosionType: EnumExplosionType_INTERNAL,
	FacialAnimationStreamingState: EnumFacialAnimationStreamingState_INTERNAL,
	FieldOfViewMode: EnumFieldOfViewMode_INTERNAL,
	FillDirection: EnumFillDirection_INTERNAL,
	FilterResult: EnumFilterResult_INTERNAL,
	FinishRecordingOperation: EnumFinishRecordingOperation_INTERNAL,
	FluidForces: EnumFluidForces_INTERNAL,
	Font: EnumFont_INTERNAL,
	FontSize: EnumFontSize_INTERNAL,
	FontStyle: EnumFontStyle_INTERNAL,
	FontWeight: EnumFontWeight_INTERNAL,
	ForceLimitMode: EnumForceLimitMode_INTERNAL,
	FormFactor: EnumFormFactor_INTERNAL,
	FrameStyle: EnumFrameStyle_INTERNAL,
	FramerateManagerMode: EnumFramerateManagerMode_INTERNAL,
	FriendRequestEvent: EnumFriendRequestEvent_INTERNAL,
	FriendStatus: EnumFriendStatus_INTERNAL,
	FunctionalTestResult: EnumFunctionalTestResult_INTERNAL,
	GameAvatarType: EnumGameAvatarType_INTERNAL,
	GearGenreSetting: EnumGearGenreSetting_INTERNAL,
	GearType: EnumGearType_INTERNAL,
	Genre: EnumGenre_INTERNAL,
	GraphicsMode: EnumGraphicsMode_INTERNAL,
	GuiType: EnumGuiType_INTERNAL,
	HandlesStyle: EnumHandlesStyle_INTERNAL,
	HighlightDepthMode: EnumHighlightDepthMode_INTERNAL,
	HorizontalAlignment: EnumHorizontalAlignment_INTERNAL,
	HoverAnimateSpeed: EnumHoverAnimateSpeed_INTERNAL,
	HttpCachePolicy: EnumHttpCachePolicy_INTERNAL,
	HttpContentType: EnumHttpContentType_INTERNAL,
	HttpError: EnumHttpError_INTERNAL,
	HttpRequestType: EnumHttpRequestType_INTERNAL,
	HumanoidCollisionType: EnumHumanoidCollisionType_INTERNAL,
	HumanoidDisplayDistanceType: EnumHumanoidDisplayDistanceType_INTERNAL,
	HumanoidHealthDisplayType: EnumHumanoidHealthDisplayType_INTERNAL,
	HumanoidOnlySetCollisionsOnStateChange: EnumHumanoidOnlySetCollisionsOnStateChange_INTERNAL,
	HumanoidRigType: EnumHumanoidRigType_INTERNAL,
	HumanoidStateMachineMode: EnumHumanoidStateMachineMode_INTERNAL,
	HumanoidStateType: EnumHumanoidStateType_INTERNAL,
	IKCollisionsMode: EnumIKCollisionsMode_INTERNAL,
	IKControlConstraintSupport: EnumIKControlConstraintSupport_INTERNAL,
	IKControlType: EnumIKControlType_INTERNAL,
	IXPLoadingStatus: EnumIXPLoadingStatus_INTERNAL,
	InOut: EnumInOut_INTERNAL,
	InfoType: EnumInfoType_INTERNAL,
	InitialDockState: EnumInitialDockState_INTERNAL,
	InputType: EnumInputType_INTERNAL,
	InterpolationThrottlingMode: EnumInterpolationThrottlingMode_INTERNAL,
	JointCreationMode: EnumJointCreationMode_INTERNAL,
	KeyCode: EnumKeyCode_INTERNAL,
	KeyInterpolationMode: EnumKeyInterpolationMode_INTERNAL,
	KeywordFilterType: EnumKeywordFilterType_INTERNAL,
	Language: EnumLanguage_INTERNAL,
	LeftRight: EnumLeftRight_INTERNAL,
	Limb: EnumLimb_INTERNAL,
	LineJoinMode: EnumLineJoinMode_INTERNAL,
	ListDisplayMode: EnumListDisplayMode_INTERNAL,
	ListenerType: EnumListenerType_INTERNAL,
	LoadCharacterLayeredClothing: EnumLoadCharacterLayeredClothing_INTERNAL,
	LoadDynamicHeads: EnumLoadDynamicHeads_INTERNAL,
	MarkupKind: EnumMarkupKind_INTERNAL,
	Material: EnumMaterial_INTERNAL,
	MaterialPattern: EnumMaterialPattern_INTERNAL,
	MembershipType: EnumMembershipType_INTERNAL,
	MeshPartDetailLevel: EnumMeshPartDetailLevel_INTERNAL,
	MeshPartHeadsAndAccessories: EnumMeshPartHeadsAndAccessories_INTERNAL,
	MeshScaleUnit: EnumMeshScaleUnit_INTERNAL,
	MeshType: EnumMeshType_INTERNAL,
	MessageType: EnumMessageType_INTERNAL,
	ModelLevelOfDetail: EnumModelLevelOfDetail_INTERNAL,
	ModelStreamingBehavior: EnumModelStreamingBehavior_INTERNAL,
	ModelStreamingMode: EnumModelStreamingMode_INTERNAL,
	ModifierKey: EnumModifierKey_INTERNAL,
	MouseBehavior: EnumMouseBehavior_INTERNAL,
	MoveState: EnumMoveState_INTERNAL,
	MuteState: EnumMuteState_INTERNAL,
	NameOcclusion: EnumNameOcclusion_INTERNAL,
	NetworkOwnership: EnumNetworkOwnership_INTERNAL,
	NormalId: EnumNormalId_INTERNAL,
	OrientationAlignmentMode: EnumOrientationAlignmentMode_INTERNAL,
	OutfitSource: EnumOutfitSource_INTERNAL,
	OutfitType: EnumOutfitType_INTERNAL,
	OutputLayoutMode: EnumOutputLayoutMode_INTERNAL,
	OverrideMouseIconBehavior: EnumOverrideMouseIconBehavior_INTERNAL,
	PackagePermission: EnumPackagePermission_INTERNAL,
	PartType: EnumPartType_INTERNAL,
	ParticleEmitterShape: EnumParticleEmitterShape_INTERNAL,
	ParticleEmitterShapeInOut: EnumParticleEmitterShapeInOut_INTERNAL,
	ParticleEmitterShapeStyle: EnumParticleEmitterShapeStyle_INTERNAL,
	ParticleFlipbookLayout: EnumParticleFlipbookLayout_INTERNAL,
	ParticleFlipbookMode: EnumParticleFlipbookMode_INTERNAL,
	ParticleFlipbookTextureCompatible: EnumParticleFlipbookTextureCompatible_INTERNAL,
	ParticleOrientation: EnumParticleOrientation_INTERNAL,
	PathStatus: EnumPathStatus_INTERNAL,
	PathWaypointAction: EnumPathWaypointAction_INTERNAL,
	PermissionLevelShown: EnumPermissionLevelShown_INTERNAL,
	PhysicsSimulationRate: EnumPhysicsSimulationRate_INTERNAL,
	PhysicsSteppingMethod: EnumPhysicsSteppingMethod_INTERNAL,
	Platform: EnumPlatform_INTERNAL,
	PlaybackState: EnumPlaybackState_INTERNAL,
	PlayerActions: EnumPlayerActions_INTERNAL,
	PlayerChatType: EnumPlayerChatType_INTERNAL,
	PoseEasingDirection: EnumPoseEasingDirection_INTERNAL,
	PoseEasingStyle: EnumPoseEasingStyle_INTERNAL,
	PositionAlignmentMode: EnumPositionAlignmentMode_INTERNAL,
	PrivilegeType: EnumPrivilegeType_INTERNAL,
	ProductLocationRestriction: EnumProductLocationRestriction_INTERNAL,
	ProductPurchaseDecision: EnumProductPurchaseDecision_INTERNAL,
	PromptCreateAssetResult: EnumPromptCreateAssetResult_INTERNAL,
	PromptPublishAssetResult: EnumPromptPublishAssetResult_INTERNAL,
	PropertyStatus: EnumPropertyStatus_INTERNAL,
	ProximityPromptExclusivity: EnumProximityPromptExclusivity_INTERNAL,
	ProximityPromptInputType: EnumProximityPromptInputType_INTERNAL,
	ProximityPromptStyle: EnumProximityPromptStyle_INTERNAL,
	QualityLevel: EnumQualityLevel_INTERNAL,
	R15CollisionType: EnumR15CollisionType_INTERNAL,
	RaycastFilterType: EnumRaycastFilterType_INTERNAL,
	RejectCharacterDeletions: EnumRejectCharacterDeletions_INTERNAL,
	RenderFidelity: EnumRenderFidelity_INTERNAL,
	RenderPriority: EnumRenderPriority_INTERNAL,
	RenderingTestComparisonMethod: EnumRenderingTestComparisonMethod_INTERNAL,
	ReplicateInstanceDestroySetting: EnumReplicateInstanceDestroySetting_INTERNAL,
	ResamplerMode: EnumResamplerMode_INTERNAL,
	ReservedHighlightId: EnumReservedHighlightId_INTERNAL,
	RestPose: EnumRestPose_INTERNAL,
	ReturnKeyType: EnumReturnKeyType_INTERNAL,
	ReverbType: EnumReverbType_INTERNAL,
	RibbonTool: EnumRibbonTool_INTERNAL,
	RigScale: EnumRigScale_INTERNAL,
	RigType: EnumRigType_INTERNAL,
	RollOffMode: EnumRollOffMode_INTERNAL,
	RotationOrder: EnumRotationOrder_INTERNAL,
	RotationType: EnumRotationType_INTERNAL,
	RtlTextSupport: EnumRtlTextSupport_INTERNAL,
	RunContext: EnumRunContext_INTERNAL,
	RuntimeUndoBehavior: EnumRuntimeUndoBehavior_INTERNAL,
	SafeAreaCompatibility: EnumSafeAreaCompatibility_INTERNAL,
	SalesTypeFilter: EnumSalesTypeFilter_INTERNAL,
	SaveAvatarThumbnailCustomizationFailure: EnumSaveAvatarThumbnailCustomizationFailure_INTERNAL,
	SaveFilter: EnumSaveFilter_INTERNAL,
	SavedQualitySetting: EnumSavedQualitySetting_INTERNAL,
	ScaleType: EnumScaleType_INTERNAL,
	ScopeCheckResult: EnumScopeCheckResult_INTERNAL,
	ScreenInsets: EnumScreenInsets_INTERNAL,
	ScreenOrientation: EnumScreenOrientation_INTERNAL,
	ScrollBarInset: EnumScrollBarInset_INTERNAL,
	ScrollingDirection: EnumScrollingDirection_INTERNAL,
	SelectionBehavior: EnumSelectionBehavior_INTERNAL,
	SelectionRenderMode: EnumSelectionRenderMode_INTERNAL,
	SelfViewPosition: EnumSelfViewPosition_INTERNAL,
	SensorMode: EnumSensorMode_INTERNAL,
	SensorUpdateType: EnumSensorUpdateType_INTERNAL,
	ServerAudioBehavior: EnumServerAudioBehavior_INTERNAL,
	ServiceVisibility: EnumServiceVisibility_INTERNAL,
	Severity: EnumSeverity_INTERNAL,
	SignalBehavior: EnumSignalBehavior_INTERNAL,
	SizeConstraint: EnumSizeConstraint_INTERNAL,
	SolverConvergenceVisualizationMode: EnumSolverConvergenceVisualizationMode_INTERNAL,
	SortDirection: EnumSortDirection_INTERNAL,
	SortOrder: EnumSortOrder_INTERNAL,
	SpecialKey: EnumSpecialKey_INTERNAL,
	StartCorner: EnumStartCorner_INTERNAL,
	Status: EnumStatus_INTERNAL,
	StreamOutBehavior: EnumStreamOutBehavior_INTERNAL,
	StreamingIntegrityMode: EnumStreamingIntegrityMode_INTERNAL,
	StreamingPauseMode: EnumStreamingPauseMode_INTERNAL,
	StudioCloseMode: EnumStudioCloseMode_INTERNAL,
	StudioDataModelType: EnumStudioDataModelType_INTERNAL,
	StudioScriptEditorColorCategories: EnumStudioScriptEditorColorCategories_INTERNAL,
	StudioScriptEditorColorPresets: EnumStudioScriptEditorColorPresets_INTERNAL,
	StudioStyleGuideColor: EnumStudioStyleGuideColor_INTERNAL,
	StudioStyleGuideModifier: EnumStudioStyleGuideModifier_INTERNAL,
	Style: EnumStyle_INTERNAL,
	SurfaceConstraint: EnumSurfaceConstraint_INTERNAL,
	SurfaceGuiShape: EnumSurfaceGuiShape_INTERNAL,
	SurfaceGuiSizingMode: EnumSurfaceGuiSizingMode_INTERNAL,
	SurfaceType: EnumSurfaceType_INTERNAL,
	SwipeDirection: EnumSwipeDirection_INTERNAL,
	TableMajorAxis: EnumTableMajorAxis_INTERNAL,
	Technology: EnumTechnology_INTERNAL,
	TeleportMethod: EnumTeleportMethod_INTERNAL,
	TeleportResult: EnumTeleportResult_INTERNAL,
	TeleportState: EnumTeleportState_INTERNAL,
	TeleportType: EnumTeleportType_INTERNAL,
	TerrainAcquisitionMethod: EnumTerrainAcquisitionMethod_INTERNAL,
	TerrainFace: EnumTerrainFace_INTERNAL,
	TextChatMessageStatus: EnumTextChatMessageStatus_INTERNAL,
	TextDirection: EnumTextDirection_INTERNAL,
	TextFilterContext: EnumTextFilterContext_INTERNAL,
	TextInputType: EnumTextInputType_INTERNAL,
	TextTruncate: EnumTextTruncate_INTERNAL,
	TextXAlignment: EnumTextXAlignment_INTERNAL,
	TextYAlignment: EnumTextYAlignment_INTERNAL,
	TextureMode: EnumTextureMode_INTERNAL,
	TextureQueryType: EnumTextureQueryType_INTERNAL,
	ThreadPoolConfig: EnumThreadPoolConfig_INTERNAL,
	ThrottlingPriority: EnumThrottlingPriority_INTERNAL,
	ThumbnailSize: EnumThumbnailSize_INTERNAL,
	ThumbnailType: EnumThumbnailType_INTERNAL,
	TickCountSampleMethod: EnumTickCountSampleMethod_INTERNAL,
	TopBottom: EnumTopBottom_INTERNAL,
	TouchCameraMovementMode: EnumTouchCameraMovementMode_INTERNAL,
	TouchMovementMode: EnumTouchMovementMode_INTERNAL,
	TrackerError: EnumTrackerError_INTERNAL,
	TrackerExtrapolationFlagMode: EnumTrackerExtrapolationFlagMode_INTERNAL,
	TrackerLodFlagMode: EnumTrackerLodFlagMode_INTERNAL,
	TrackerLodValueMode: EnumTrackerLodValueMode_INTERNAL,
	TrackerMode: EnumTrackerMode_INTERNAL,
	TrackerPromptEvent: EnumTrackerPromptEvent_INTERNAL,
	TriStateBoolean: EnumTriStateBoolean_INTERNAL,
	TweenStatus: EnumTweenStatus_INTERNAL,
	UITheme: EnumUITheme_INTERNAL,
	UiMessageType: EnumUiMessageType_INTERNAL,
	UsageContext: EnumUsageContext_INTERNAL,
	UserCFrame: EnumUserCFrame_INTERNAL,
	UserInputState: EnumUserInputState_INTERNAL,
	UserInputType: EnumUserInputType_INTERNAL,
	VRSafetyBubbleMode: EnumVRSafetyBubbleMode_INTERNAL,
	VRScaling: EnumVRScaling_INTERNAL,
	VRSessionState: EnumVRSessionState_INTERNAL,
	VRTouchpad: EnumVRTouchpad_INTERNAL,
	VRTouchpadMode: EnumVRTouchpadMode_INTERNAL,
	VelocityConstraintMode: EnumVelocityConstraintMode_INTERNAL,
	VerticalAlignment: EnumVerticalAlignment_INTERNAL,
	VerticalScrollBarPosition: EnumVerticalScrollBarPosition_INTERNAL,
	VibrationMotor: EnumVibrationMotor_INTERNAL,
	ViewMode: EnumViewMode_INTERNAL,
	VirtualCursorMode: EnumVirtualCursorMode_INTERNAL,
	VirtualInputMode: EnumVirtualInputMode_INTERNAL,
	VoiceChatState: EnumVoiceChatState_INTERNAL,
	VolumetricAudio: EnumVolumetricAudio_INTERNAL,
	WaterDirection: EnumWaterDirection_INTERNAL,
	WaterForce: EnumWaterForce_INTERNAL,
	WeldConstraintPreserve: EnumWeldConstraintPreserve_INTERNAL,
	WrapLayerAutoSkin: EnumWrapLayerAutoSkin_INTERNAL,
	WrapLayerDebugMode: EnumWrapLayerDebugMode_INTERNAL,
	WrapTargetDebugMode: EnumWrapTargetDebugMode_INTERNAL,
	ZIndexBehavior: EnumZIndexBehavior_INTERNAL,
} & { GetEnums: (self: ENUM_LIST) -> { Enum } }
declare Enum: ENUM_LIST

declare class Axes
	Back: boolean
	Bottom: boolean
	Front: boolean
	Left: boolean
	Right: boolean
	Top: boolean
	X: boolean
	Y: boolean
	Z: boolean
end

declare class CatalogSearchParams
	AssetTypes: { EnumAssetType }
	BundleType: { EnumBundleType }
	CategoryFilter: EnumCatalogCategoryFilter
	MaxPrice: number
	MinPrice: number
	SearchKeyword: string
	SortType: EnumCatalogSortType
end

declare class Color3
	B: number
	G: number
	R: number
	function Lerp(self, color: Color3, alpha: number): Color3
	function ToHSV(self): (number, number, number)
	function ToHex(self): string
end

declare class BrickColor
	Color: Color3
	Name: string
	Number: number
	b: number
	g: number
	r: number
end

declare class ColorSequenceKeypoint
	Time: number
	Value: Color3
end

declare class ColorSequence
	Keypoints: { ColorSequenceKeypoint }
end

declare class DateTime
	UnixTimestamp: number
	UnixTimestampMillis: number
	function FormatLocalTime(self, format: string, locale: string): string
	function FormatUniversalTime(self, format: string, locale: string): string
	function ToIsoDate(self): string
	function ToLocalTime(self): { any }
	function ToUniversalTime(self): { any }
end

declare class DockWidgetPluginGuiInfo
	FloatingXSize: number
	FloatingYSize: number
	InitialEnabled: boolean
	InitialEnabledShouldOverrideRestore: boolean
	MinHeight: number
	MinWidth: number
end





declare class Enums
	function GetEnums(self): { Enum }
end

declare class Faces
	Back: boolean
	Bottom: boolean
	Front: boolean
	Left: boolean
	Right: boolean
	Top: boolean
end

declare class FloatCurveKey
	Interpolation: EnumKeyInterpolationMode
	LeftTangent: number
	RightTangent: number
	Time: number
	Value: number
end

declare class Font
	Bold: boolean
	Family: string
	Style: EnumFontStyle
	Weight: EnumFontWeight
end

declare class NumberRange
	Max: number
	Min: number
end

declare class NumberSequenceKeypoint
	Envelope: number
	Time: number
	Value: number
end

declare class NumberSequence
	Keypoints: { NumberSequenceKeypoint }
end

declare class OverlapParams
	CollisionGroup: string
	FilterDescendantsInstances: { Instance }
	FilterType: EnumRaycastFilterType
	MaxParts: number
	RespectCanCollide: boolean
end

declare class PhysicalProperties
	Density: number
	Elasticity: number
	ElasticityWeight: number
	Friction: number
	FrictionWeight: number
end

declare class RBXScriptConnection
	Connected: boolean
	function Disconnect(self): nil
end



declare class RaycastParams
	CollisionGroup: string
	FilterDescendantsInstances: { Instance }
	FilterType: EnumRaycastFilterType
	IgnoreWater: boolean
	RespectCanCollide: boolean
end

declare class TweenInfo
	DelayTime: number
	EasingDirection: EnumEasingDirection
	EasingStyle: EnumEasingStyle
	RepeatCount: number
	Reverses: boolean
	Time: number
end

declare class UDim
	Offset: number
	Scale: number
	function __add(self, other: UDim): UDim
	function __sub(self, other: UDim): UDim
	function __unm(self): UDim
end

declare class UDim2
	Height: UDim
	Width: UDim
	X: UDim
	Y: UDim
	function Lerp(self, goal: UDim2, alpha: number): UDim2
	function __add(self, other: UDim2): UDim2
	function __sub(self, other: UDim2): UDim2
	function __unm(self): UDim2
end

declare class Vector2
	Magnitude: number
	Unit: Vector2
	X: number
	Y: number
	function Cross(self, other: Vector2): number
	function Dot(self, v: Vector2): number
	function Lerp(self, v: Vector2, alpha: number): Vector2
	function __add(self, other: Vector2): Vector2
	function __div(self, other: Vector2 | number): Vector2
	function __mul(self, other: Vector2 | number): Vector2
	function __sub(self, other: Vector2): Vector2
	function __unm(self): Vector2
end

declare class Rect
	Height: number
	Max: Vector2
	Min: Vector2
	Width: number
end

declare class Vector2int16
	X: number
	Y: number
	function __add(self, other: Vector2int16): Vector2int16
	function __div(self, other: Vector2int16 | number): Vector2int16
	function __mul(self, other: Vector2int16 | number): Vector2int16
	function __sub(self, other: Vector2int16): Vector2int16
	function __unm(self): Vector2int16
end

declare class Vector3
	Magnitude: number
	Unit: Vector3
	X: number
	Y: number
	Z: number
	function Angle(self, other: Vector3, axis: Vector3?): number
	function Cross(self, other: Vector3): Vector3
	function Dot(self, other: Vector3): number
	function FuzzyEq(self, other: Vector3, epsilon: number): boolean
	function Lerp(self, goal: Vector3, alpha: number): Vector3
	function __add(self, other: Vector3): Vector3
	function __div(self, other: Vector3 | number): Vector3
	function __mul(self, other: Vector3 | number): Vector3
	function __sub(self, other: Vector3): Vector3
	function __unm(self): Vector3
end

declare class RaycastResult
	Distance: number
	Instance: Instance
	Material: EnumMaterial
	Normal: Vector3
	Position: Vector3
end

declare class Ray
	Direction: Vector3
	Origin: Vector3
	Unit: Ray
	function ClosestPoint(self, point: Vector3): Vector3
	function Distance(self, point: Vector3): number
end

declare class Random
	function Clone(self): Random
	function NextInteger(self, min: number, max: number): number
	function NextNumber(self): number
	function NextNumber(self, min: number, max: number): number
	function NextUnitVector(self): Vector3
end

declare class PathWaypoint
	Action: EnumPathWaypointAction
	Position: Vector3
end

declare class CFrame
	LookVector: Vector3
	Position: Vector3
	RightVector: Vector3
	Rotation: CFrame
	UpVector: Vector3
	X: number
	XVector: Vector3
	Y: number
	YVector: Vector3
	Z: number
	ZVector: Vector3
	function GetComponents(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function Inverse(self): CFrame
	function Lerp(self, goal: CFrame, alpha: number): CFrame
	function Orthonormalize(self): CFrame
	function PointToObjectSpace(self, v3: Vector3): Vector3
	function PointToWorldSpace(self, v3: Vector3): Vector3
	function ToAxisAngle(self): (Vector3, number)
	function ToEulerAnglesXYZ(self): (number, number, number)
	function ToEulerAnglesYXZ(self): (number, number, number)
	function ToObjectSpace(self, cf: CFrame): CFrame
	function ToOrientation(self): (number, number, number)
	function ToWorldSpace(self, cf: CFrame): CFrame
	function VectorToObjectSpace(self, v3: Vector3): Vector3
	function VectorToWorldSpace(self, v3: Vector3): Vector3
	function __add(self, other: Vector3): CFrame
	function __mul(self, other: CFrame): CFrame
	function __mul(self, other: Vector3): Vector3
	function __sub(self, other: Vector3): CFrame
end

declare class RotationCurveKey
	Interpolation: EnumKeyInterpolationMode
	LeftTangent: number
	RightTangent: number
	Time: number
	Value: CFrame
end

declare class Region3
	CFrame: CFrame
	Size: Vector3
	function ExpandToGrid(self, Region: number): Region3
end

declare class Vector3int16
	X: number
	Y: number
	Z: number
	function __add(self, other: Vector3int16): Vector3int16
	function __div(self, other: Vector3int16 | number): Vector3int16
	function __mul(self, other: Vector3int16 | number): Vector3int16
	function __sub(self, other: Vector3int16): Vector3int16
	function __unm(self): Vector3int16
end

declare class Region3int16
	Max: Vector3int16
	Min: Vector3int16
end


declare SharedTable: any
declare OpenCloudModel: any

export type RBXScriptSignal<T... = ...any> = {
    Wait: (self: RBXScriptSignal<T...>) -> T...,
    Connect: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    ConnectParallel: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    Once: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
}

type HttpRequestOptions = {
    Url: string,
    Method: "GET" | "HEAD" | "POST" | "PUT" | "DELETE" | "CONNECT" | "OPTIONS" | "TRACE" | "PATCH" | nil,
    Headers: { [string]: string }?,
    Body: string?,
}

type HttpResponseData = {
    Success: boolean,
    StatusCode: number,
    StatusMessage: string,
    Headers: { [string]: string },
    Body: string?,
}

type HumanoidDescriptionAccessory = {
    AssetId: number,
    AccessoryType: EnumAccessoryType,
    IsLayered: boolean,
    Order: number?,
    Puffiness: number?,
}

type Hat = any
type AnalyticsService = any
type RocketPropulsion = any
type Skin = any
type Plane = any
type CustomEvent = any
type CustomEventReceiver = any
type BevelMesh = any
type CylinderMesh = any
type Hole = any
type MotorFeature = any
type FunctionalTest = any
type GuiMain = any
type FloorWire = any
type SelectionPartLasso = any
type SelectionPointLasso = any
type Hopper = any
type RotateP = any
type RotateV = any
type Glue = any
type ManualSurfaceJointInstance = any
type ManualGlue = any
type ManualWeld = any
type Rotate = any
type Snap = any
type JointsService = any
type Message = any
type Hint = any
type FlagStand = any
type SkateboardPlatform = any
type HopperBin = any
type Flag = any
type Status = any
type PointsService = any
type DoubleConstrainedValue = any
type IntConstrainedValue = any
declare class Instance
	AncestryChanged: RBXScriptSignal<Instance, Instance?>
	Archivable: boolean
	AttributeChanged: RBXScriptSignal<string>
	Changed: RBXScriptSignal<string>
	ChildAdded: RBXScriptSignal<Instance>
	ChildRemoved: RBXScriptSignal<Instance>
	ClassName: string
	DescendantAdded: RBXScriptSignal<Instance>
	DescendantRemoving: RBXScriptSignal<Instance>
	Destroying: RBXScriptSignal<>
	Name: string
	Parent: Instance?
	RobloxLocked: boolean
	SourceAssetId: number
	function AddTag(self, tag: string): nil
	function ClearAllChildren(self): nil
	function Clone(self): Instance
	function Destroy(self): nil
	function FindFirstAncestor(self, name: string): Instance?
	function FindFirstAncestorOfClass(self, className: string): Instance?
	function FindFirstAncestorWhichIsA(self, className: string): Instance?
	function FindFirstChild(self, name: string, recursive: boolean?): Instance?
	function FindFirstChildOfClass(self, className: string): Instance?
	function FindFirstChildWhichIsA(self, className: string, recursive: boolean?): Instance?
	function FindFirstDescendant(self, name: string): Instance?
	function GetActor(self): Actor?
	function GetAttribute(self, attribute: string): any
	function GetAttributeChangedSignal(self, attribute: string): RBXScriptSignal<>
	function GetAttributes(self): { [string]: any }
	function GetChildren(self): { Instance }
	function GetDebugId(self, scopeLength: number?): string
	function GetDescendants(self): { Instance }
	function GetFullName(self): string
	function GetPropertyChangedSignal(self, property: string): RBXScriptSignal<>
	function GetTags(self): { any }
	function HasTag(self, tag: string): boolean
	function IsA(self, className: string): boolean
	function IsAncestorOf(self, descendant: Instance): boolean
	function IsDescendantOf(self, ancestor: Instance): boolean
	function IsPropertyModified(self, name: string): boolean
	function RemoveTag(self, tag: string): nil
	function ResetPropertyToDefault(self, name: string): nil
	function SetAttribute(self, attribute: string, value: any): nil
	function WaitForChild(self, name: string): Instance
	function WaitForChild(self, name: string, timeout: number): Instance?
end

declare class Accoutrement extends Instance
	AttachmentForward: Vector3
	AttachmentPoint: CFrame
	AttachmentPos: Vector3
	AttachmentRight: Vector3
	AttachmentUp: Vector3
end

declare class Accessory extends Accoutrement
	AccessoryType: EnumAccessoryType
end



declare class AdPortal extends Instance
	PortalInvalidReason: string
	PortalVersion: number
	Status: EnumAdUnitStatus
	function TeleportConfirmed(self, placeId: number, player: Player): nil
	function TeleportRejected(self, shouldCooldown: boolean, rejectedByDistance: boolean): nil
end

declare class AdService extends Instance
	AdTeleportEnded: RBXScriptSignal<>
	AdTeleportInitiated: RBXScriptSignal<>
	PortalPrompt: RBXScriptSignal<number, Instance, boolean>
	function GetAdTeleportInfo(self): any
	function GetReportAdInfo(self): { any }
	function ReturnToPublisherExperience(self, adTeleportMethod: EnumAdTeleportMethod): nil
end

declare class AdvancedDragger extends Instance
end



declare class Animation extends Instance
	AnimationId: Content
end

declare class AnimationClip extends Instance
	Guid: string
	Loop: boolean
	Priority: EnumAnimationPriority
end

declare class CurveAnimation extends AnimationClip
end

declare class KeyframeSequence extends AnimationClip
	AuthoredHipHeight: number
	function AddKeyframe(self, keyframe: Keyframe): nil
	function GetKeyframes(self): { Instance }
	function RemoveKeyframe(self, keyframe: Keyframe): nil
end

declare class AnimationClipProvider extends Instance
	function GetAnimationClipAsync(self, assetId: Content): AnimationClip
	function GetAnimations(self, userId: number): Instance
	function GetMemStats(self): { [any]: any }
	function RegisterActiveAnimationClip(self, animationClip: AnimationClip): Content
	function RegisterAnimationClip(self, animationClip: AnimationClip): Content
end

declare class AnimationController extends Instance
end

declare class AnimationFromVideoCreatorService extends Instance
	function CreateJob(self, filePath: string): string
	function DownloadJobResult(self, jobId: string, outputFilePath: string): string
	function FullProcess(self, videoFilePath: string, progressCallback: ((...any) -> ...any)): string
	function GetJobStatus(self, jobId: string): string
end

declare class AnimationFromVideoCreatorStudioService extends Instance
	function CreateAnimationByUploadingVideo(self, progressCallback: ((...any) -> ...any)): string
	function ImportVideoWithPrompt(self): string
	function IsAgeRestricted(self): boolean
end

declare class AnimationRigData extends Instance
	function LoadFromHumanoid(self, humanoid: Instance): boolean
end

declare class AnimationStreamTrack extends Instance
	Animation: TrackerStreamAnimation
	IsPlaying: boolean
	Priority: EnumAnimationPriority
	Stopped: RBXScriptSignal<>
	WeightCurrent: number
	WeightTarget: number
	function AdjustWeight(self, weight: number?, fadeTime: number?): nil
	function GetTrackerData(self): any
	function Play(self, fadeTime: number?, weight: number?): nil
	function Stop(self, fadeTime: number?): nil
	function TogglePause(self, paused: boolean): nil
end

declare class AnimationTrack extends Instance
	Animation: Animation
	DidLoop: RBXScriptSignal<>
	Ended: RBXScriptSignal<>
	IsPlaying: boolean
	KeyframeReached: RBXScriptSignal<string>
	Length: number
	Looped: boolean
	Priority: EnumAnimationPriority
	Speed: number
	Stopped: RBXScriptSignal<>
	TimePosition: number
	WeightCurrent: number
	WeightTarget: number
	function AdjustSpeed(self, speed: number?): nil
	function AdjustWeight(self, weight: number?, fadeTime: number?): nil
	function GetMarkerReachedSignal(self, name: string): RBXScriptSignal
	function GetTimeOfKeyframe(self, keyframeName: string): number
	function Play(self, fadeTime: number?, weight: number?, speed: number?): nil
	function Stop(self, fadeTime: number?): nil
end

declare class Animator extends Instance
	AnimationPlayed: RBXScriptSignal<AnimationTrack>
	AnimationPlayedCoreScript: RBXScriptSignal<AnimationTrack>
	AnimationStreamTrackPlayed: RBXScriptSignal<AnimationStreamTrack>
	EvaluationThrottled: boolean
	PreferLodEnabled: boolean
	function ApplyJointVelocities(self, motors: any): nil
	function GetPlayingAnimationTracks(self): { AnimationTrack }
	function GetPlayingAnimationTracksCoreScript(self): { any }
	function LoadAnimation(self, animation: Animation): AnimationTrack
	function LoadAnimationCoreScript(self, animation: Animation): AnimationTrack
	function LoadStreamAnimation(self, animation: TrackerStreamAnimation): AnimationStreamTrack
	function LoadStreamAnimationForSelfieView_deprecated(self, animation: TrackerStreamAnimation, player: Player): AnimationStreamTrack
	function StepAnimations(self, deltaTime: number): nil
end

declare class AppUpdateService extends Instance
	function CheckForUpdate(self, handler: ((...any) -> ...any)?): nil
	function DisableDUAR(self): nil
	function DisableDUARAndOpenSurvey(self, surveyUrl: string): nil
	function PerformManagedUpdate(self): boolean
end

declare class AssetCounterService extends Instance
end

declare class AssetDeliveryProxy extends Instance
	Interface: string
	Port: number
	StartServer: boolean
end

declare class AssetImportService extends Instance
	function GetAllTemplates(self): { [any]: any }
	function GetTemplate(self, name: string): { [any]: any }
	function PickFileWithPrompt(self): string
	function PickMultipleFilesWithPrompt(self): { any }
	function RemoveTemplate(self, name: string): nil
	function SaveTemplate(self, name: string, importTemplate: { [any]: any }): nil
	function StartSessionWithPath(self, filePath: string): AssetImportSession
	function StartSessionWithPrompt(self): AssetImportSession
end

declare class AssetImportSession extends Instance
	UploadComplete: RBXScriptSignal<{ [any]: any }>
	UploadCompleteDeprecated: RBXScriptSignal<boolean, { [any]: any }>
	UploadProgress: RBXScriptSignal<number>
	function ApplySettings(self): nil
	function ApplyTemplate(self, template: { [any]: any }): nil
	function Cancel(self): nil
	function GetCurrentStatusTable(self): { [any]: any }
	function GetFilename(self): string
	function GetImportTree(self): Instance
	function GetInstance(self, nodeId: number): Instance
	function HasAnimation(self): boolean
	function IsAvatar(self): boolean
	function IsGltf(self): boolean
	function IsR15(self): boolean
	function Reset(self): nil
	function Upload(self): nil
	function usesCustomRestPoseLua(self): boolean
end

declare class AssetManagerService extends Instance
	AssetImportedSignal: RBXScriptSignal<EnumAssetType, string, number>
	ImportSessionFinished: RBXScriptSignal<>
	ImportSessionStarted: RBXScriptSignal<>
	MayBeLinkedSourceModified: RBXScriptSignal<string>
	function AddNewPlace(self): number
	function CreateAlias(self, assetType: number, assetId: number, aliasName: string): nil
	function DeleteAlias(self, aliasName: string): nil
	function GetMeshIdFromAliasName(self, aliasName: string): number
	function GetMeshIdFromAssetId(self, assetId: number): number
	function GetTextureIdFromAliasName(self, aliasName: string): number
	function GetTextureIdFromAssetId(self, assetId: number): number
	function HasUnpublishedChangesForLinkedSource(self, aliasName: string): boolean
	function InsertAudio(self, assetId: number, assetName: string): nil
	function InsertImage(self, assetId: number): nil
	function InsertLinkedSourceAsLocalScript(self, aliasName: string): nil
	function InsertLinkedSourceAsModuleScript(self, aliasName: string): nil
	function InsertLinkedSourceAsScript(self, aliasName: string): nil
	function InsertMesh(self, aliasName: string, insertWithLocation: boolean): nil
	function InsertMeshesWithLocation(self, aliasNames: { any }): nil
	function InsertModel(self, modelId: number): nil
	function InsertPackage(self, packageId: number): nil
	function InsertVideo(self, assetId: number, assetName: string): nil
	function OpenLinkedSource(self, aliasName: string): nil
	function OpenPlace(self, placeId: number): nil
	function PublishLinkedSource(self, assetId: number, aliasName: string): nil
	function RefreshLinkedSource(self, aliasName: string): nil
	function RemovePlace(self, placeId: number): nil
	function RenameAlias(self, assetType: number, assetId: number, oldAliasName: string, newAliasName: string): nil
	function RenameModel(self, modelId: number, newName: string): nil
	function RenamePlace(self, placeId: number, newName: string): nil
	function RevertLinkedSourceToLastPublishedVersion(self, aliasName: string): nil
	function ShowPackageDetails(self, packageId: number): nil
	function UpdateAllPackages(self, packageId: number): nil
	function ViewPackageOnWebsite(self, packageId: number): nil
end

declare class AssetPatchSettings extends Instance
	ContentId: string
	OutputPath: string
	PatchId: string
end

declare class AssetService extends Instance
	OpenCreateResultModal: RBXScriptSignal<EnumPromptCreateAssetResult>
	OpenPublishResultModal: RBXScriptSignal<EnumPromptPublishAssetResult>
	function CreatePlaceAsync(self, placeName: string, templatePlaceID: number, description: string?): number
	function CreatePlaceInPlayerInventoryAsync(self, player: Player, placeName: string, templatePlaceID: number, description: string?): number
	function DeserializeInstance(self, serializedInstance: string): Instance
	function GetAssetIdsForPackage(self, packageAssetId: number): { any }
	function GetBundleDetailsAsync(self, bundleId: number): { [any]: any }
	function GetBundleDetailsSync(self, bundleId: number): { [any]: any }
	function GetGamePlacesAsync(self): Instance
	function LoadImageAsync(self, textureId: Content): DynamicImage
	function PromptCreateAssetAsync(self, player: Player, instance: Instance, assetType: EnumAssetType): any
	function SavePlaceAsync(self): nil
	function SearchAudio(self, searchParameters: AudioSearchParams): AudioPages
end

declare class Atmosphere extends Instance
	Color: Color3
	Decay: Color3
	Density: number
	Glare: number
	Haze: number
	Offset: number
end

declare class Attachment extends Instance
	Axis: Vector3
	CFrame: CFrame
	Orientation: Vector3
	Position: Vector3
	SecondaryAxis: Vector3
	Visible: boolean
	WorldAxis: Vector3
	WorldCFrame: CFrame
	WorldOrientation: Vector3
	WorldPosition: Vector3
	WorldSecondaryAxis: Vector3
	function GetConstraints(self): { Instance }
end

declare class Bone extends Attachment
	Transform: CFrame
	TransformedCFrame: CFrame
	TransformedWorldCFrame: CFrame
end

declare class AudioSearchParams extends Instance
	Album: string
	Artist: string
	AudioSubType: EnumAudioSubType
	MaxDuration: number
	MinDuration: number
	SearchKeyword: string
	Tag: string
	Title: string
end

declare class AvatarChatService extends Instance
	ClientFeatures: number
	ClientFeaturesInitialized: boolean
	ServerFeatures: number
	function GetClientFeaturesAsync(self): number
	function GetServerFeaturesAsync(self): number
	function IsEnabled(self, mask: number, feature: EnumAvatarChatServiceFeature): boolean
	function IsPlaceEnabled(self): boolean
	function IsUniverseEnabled(self): boolean
	function PollClientFeatures(self): number
	function PollServerFeatures(self): number
end

declare class AvatarEditorService extends Instance
	OpenAllowInventoryReadAccess: RBXScriptSignal<>
	OpenPromptCreateOufit: RBXScriptSignal<HumanoidDescription, EnumHumanoidRigType>
	OpenPromptDeleteOutfit: RBXScriptSignal<number>
	OpenPromptRenameOutfit: RBXScriptSignal<number>
	OpenPromptSaveAvatar: RBXScriptSignal<HumanoidDescription, EnumHumanoidRigType>
	OpenPromptSetFavorite: RBXScriptSignal<number, EnumAvatarItemType, boolean>
	OpenPromptUpdateOutfit: RBXScriptSignal<number, HumanoidDescription, EnumHumanoidRigType>
	PromptAllowInventoryReadAccessCompleted: RBXScriptSignal<EnumAvatarPromptResult>
	PromptCreateOutfitCompleted: RBXScriptSignal<EnumAvatarPromptResult, any>
	PromptDeleteOutfitCompleted: RBXScriptSignal<EnumAvatarPromptResult>
	PromptRenameOutfitCompleted: RBXScriptSignal<EnumAvatarPromptResult>
	PromptSaveAvatarCompleted: RBXScriptSignal<EnumAvatarPromptResult, HumanoidDescription>
	PromptSaveAvatarThumbnailCustomizationCompleted: RBXScriptSignal<EnumAvatarPromptResult, any>
	PromptSetFavoriteCompleted: RBXScriptSignal<EnumAvatarPromptResult>
	PromptUpdateOutfitCompleted: RBXScriptSignal<EnumAvatarPromptResult>
	function CheckApplyDefaultClothing(self, humanoidDescription: HumanoidDescription): HumanoidDescription
	function ConformToAvatarRules(self, humanoidDescription: HumanoidDescription): HumanoidDescription
	function GetAccessoryType(self, avatarAssetType: EnumAvatarAssetType): EnumAccessoryType
	function GetAvatarRules(self): { [any]: any }
	function GetBatchItemDetails(self, itemIds: { any }, itemType: EnumAvatarItemType): { any }
	function GetFavorite(self, itemId: number, itemType: EnumAvatarItemType): boolean
	function GetInventory(self, assetTypes: { any }): InventoryPages
	function GetItemDetails(self, itemId: number, itemType: EnumAvatarItemType): { [any]: any }
	function GetOutfitDetails(self, outfitId: number): { [any]: any }
	function GetOutfits(self, outfitSource: EnumOutfitSource?, outfitType: EnumOutfitType?): OutfitPages
	function GetRecommendedAssets(self, assetType: EnumAvatarAssetType, contextAssetId: number?): { any }
	function GetRecommendedBundles(self, bundleId: number): { any }
	function NoPromptCreateOutfit(self, humanoidDescription: HumanoidDescription, rigType: EnumHumanoidRigType, name: string): boolean
	function NoPromptDeleteOutfit(self, outfitId: number): boolean
	function NoPromptRenameOutfit(self, outfitId: number, name: string): boolean
	function NoPromptSaveAvatar(self, humanoidDescription: HumanoidDescription, rigType: EnumHumanoidRigType, saveDict: { [any]: any }, gearAssetId: number?): boolean
	function NoPromptSaveAvatarThumbnailCustomization(self, thumbnailType: EnumAvatarThumbnailCustomizationType, emoteAssetId: number, cameraDistanceScale: number, yRotDeg: number, fieldOfViewDeg: number?): boolean
	function NoPromptSetFavorite(self, itemId: number, itemType: EnumAvatarItemType, shouldFavorite: boolean): boolean
	function NoPromptUpdateOutfit(self, outfitId: number, humanoidDescription: HumanoidDescription, rigType: EnumHumanoidRigType): boolean
	function PerformCreateOutfitWithDescription(self, humanoidDescription: HumanoidDescription, name: string): nil
	function PerformDeleteOutfit(self): nil
	function PerformRenameOutfit(self, name: string): nil
	function PerformSaveAvatarWithDescription(self, humanoidDescription: HumanoidDescription, addedAssets: { any }, removedAssets: { any }): nil
	function PerformSetFavorite(self): nil
	function PerformUpdateOutfit(self, humanoidDescription: HumanoidDescription): nil
	function PromptAllowInventoryReadAccess(self): nil
	function PromptCreateOutfit(self, outfit: HumanoidDescription, rigType: EnumHumanoidRigType): nil
	function PromptDeleteOutfit(self, outfitId: number): nil
	function PromptRenameOutfit(self, outfitId: number): nil
	function PromptSaveAvatar(self, humanoidDescription: HumanoidDescription, rigType: EnumHumanoidRigType): nil
	function PromptSetFavorite(self, itemId: number, itemType: EnumAvatarItemType, shouldFavorite: boolean): nil
	function PromptUpdateOutfit(self, outfitId: number, updatedOutfit: HumanoidDescription, rigType: EnumHumanoidRigType): nil
	function SearchCatalog(self, searchParameters: CatalogSearchParams): CatalogPages
	function SetAllowInventoryReadAccess(self, inventoryReadAccessGranted: boolean): nil
	function SignalCreateOutfitFailed(self): nil
	function SignalCreateOutfitPermissionDenied(self): nil
	function SignalDeleteOutfitFailed(self): nil
	function SignalDeleteOutfitPermissionDenied(self): nil
	function SignalRenameOutfitFailed(self): nil
	function SignalRenameOutfitPermissionDenied(self): nil
	function SignalSaveAvatarFailed(self): nil
	function SignalSaveAvatarPermissionDenied(self): nil
	function SignalSetFavoriteFailed(self): nil
	function SignalSetFavoritePermissionDenied(self): nil
	function SignalUpdateOutfitFailed(self): nil
	function SignalUpdateOutfitPermissionDenied(self): nil
end

declare class AvatarImportService extends Instance
	function ImportFBXAnimationFromFilePathUserMayChooseModel(self, fbxFilePath: string, selectedRig: Instance, userChooseModelThenImportCB: ((...any) -> ...any)): Instance
	function ImportFBXAnimationUserMayChooseModel(self, selectedRig: Instance, userChooseModelThenImportCB: ((...any) -> ...any)): Instance
	function ImportFbxRigWithoutSceneLoad(self, isR15: boolean?): Instance
	function ImportLoadedFBXAnimation(self, useFBXModel: boolean): Instance
	function LoadRigAndDetectType(self, promptR15Callback: ((...any) -> ...any)): Instance
end

declare class Backpack extends Instance
end

declare class BadgeService extends Instance
	BadgeAwarded: RBXScriptSignal<string, number, number>
	OnBadgeAwarded: RBXScriptSignal<number, number, number>
	function AwardBadge(self, userId: number, badgeId: number): boolean
	function GetBadgeInfoAsync(self, badgeId: number): { [any]: any }
	function UserHasBadgeAsync(self, userId: number, badgeId: number): boolean
end

declare class BaseImportData extends Instance
	Id: string
	ImportName: string
	ShouldImport: boolean
	function CreateTemplateFromData(self): { [any]: any }
	function GetStatuses(self): { [any]: any }
end

declare class AnimationImportData extends BaseImportData
end

declare class FacsImportData extends BaseImportData
end

declare class GroupImportData extends BaseImportData
	Anchored: boolean
	ImportAsModelAsset: boolean
	InsertInWorkspace: boolean
end

declare class JointImportData extends BaseImportData
end

declare class MaterialImportData extends BaseImportData
	DiffuseFilePath: string
	IsPbr: boolean
	MetalnessFilePath: string
	NormalFilePath: string
	RoughnessFilePath: string
end

declare class MeshImportData extends BaseImportData
	Anchored: boolean
	CageManifold: boolean
	CageMeshIntersectedPreview: boolean
	CageMeshNotIntersected: boolean
	CageNoOverlappingVertices: boolean
	CageNonManifoldPreview: boolean
	CageOverlappingVerticesPreview: boolean
	CageUVMatched: boolean
	CageUVMisMatchedPreview: boolean
	Dimensions: Vector3
	DoubleSided: boolean
	IgnoreVertexColors: boolean
	IrrelevantCageModifiedPreview: boolean
	MeshHoleDetectedPreview: boolean
	MeshNoHoleDetected: boolean
	NoIrrelevantCageModified: boolean
	NoOuterCageFarExtendedFromMesh: boolean
	OuterCageFarExtendedFromMeshPreview: boolean
	PolygonCount: number
	UseImportedPivot: boolean
end

declare class RootImportData extends BaseImportData
	AddModelToInventory: boolean
	Anchored: boolean
	AnimationIdForRestPose: number
	ExistingPackageId: string
	FileDimensions: Vector3
	ImportAsModelAsset: boolean
	ImportAsPackage: boolean
	InsertInWorkspace: boolean
	InsertWithScenePosition: boolean
	InvertNegativeFaces: boolean
	MergeMeshes: boolean
	PolygonCount: number
	RestPose: EnumRestPose
	RigScale: EnumRigScale
	RigType: EnumRigType
	RigVisualization: boolean
	ScaleUnit: EnumMeshScaleUnit
	UseSceneOriginAsCFrame: boolean
	UseSceneOriginAsPivot: boolean
	UsesCages: boolean
	WorldForward: EnumNormalId
	WorldUp: EnumNormalId
end

declare class BasePlayerGui extends Instance
	function GetGuiObjectsAtPosition(self, x: number, y: number): { GuiObject }
	function GetGuiObjectsInCircle(self, position: Vector2, radius: number): { GuiObject }
end

declare class CoreGui extends BasePlayerGui
	SelectionImageObject: GuiObject
	UserGuiRenderingChanged: RBXScriptSignal<boolean, Instance, EnumNormalId, number>
	Version: number
	function SetUserGuiRendering(self, enabled: boolean, guiAdornee: Instance, faceId: EnumNormalId, horizontalCurvature: number?): nil
	function TakeScreenshot(self): nil
	function ToggleRecording(self): nil
end

declare class PlayerGui extends BasePlayerGui
	CurrentScreenOrientation: EnumScreenOrientation
	ScreenOrientation: EnumScreenOrientation
	SelectionImageObject: GuiObject
end

declare class StarterGui extends BasePlayerGui
	CoreGuiChangedSignal: RBXScriptSignal<EnumCoreGuiType, boolean>
	ProcessUserInput: boolean
	RtlTextSupport: EnumRtlTextSupport
	ScreenOrientation: EnumScreenOrientation
	ShowDevelopmentGui: boolean
	VirtualCursorMode: EnumVirtualCursorMode
	function GetCore(self, parameterName: string): any
	function GetCoreGuiEnabled(self, coreGuiType: EnumCoreGuiType): boolean
	function RegisterGetCore(self, parameterName: string, getFunction: ((...any) -> ...any)): nil
	function RegisterSetCore(self, parameterName: string, setFunction: ((...any) -> ...any)): nil
	function SetCore(self, parameterName: string, value: any): nil
	function SetCoreGuiEnabled(self, coreGuiType: EnumCoreGuiType, enabled: boolean): nil
end

declare class BaseWrap extends Instance
	CageMeshId: Content
	CageOrigin: CFrame
	CageOriginWorld: CFrame
	HSRAssetId: Content
	ImportOrigin: CFrame
	ImportOriginWorld: CFrame
	function GetFaces(self, cageType: EnumCageType): { any }
	function GetVertices(self, cageType: EnumCageType): { any }
	function IsHSRReady(self): boolean
	function ModifyVertices(self, cageType: EnumCageType, vertices: { any }): nil
end

declare class WrapLayer extends BaseWrap
	AutoSkin: EnumWrapLayerAutoSkin
	BindOffset: CFrame
	Color: Color3
	DebugMode: EnumWrapLayerDebugMode
	Enabled: boolean
	Order: number
	Puffiness: number
	ReferenceMeshId: Content
	ReferenceOrigin: CFrame
	ReferenceOriginWorld: CFrame
	ShrinkFactor: number
end

declare class WrapTarget extends BaseWrap
	Color: Color3
	DebugMode: EnumWrapTargetDebugMode
	Stiffness: number
end

declare class Beam extends Instance
	Attachment0: Attachment
	Attachment1: Attachment
	Brightness: number
	Color: ColorSequence
	CurveSize0: number
	CurveSize1: number
	Enabled: boolean
	FaceCamera: boolean
	LightEmission: number
	LightInfluence: number
	Segments: number
	Texture: Content
	TextureLength: number
	TextureMode: EnumTextureMode
	TextureSpeed: number
	Transparency: NumberSequence
	Width0: number
	Width1: number
	ZOffset: number
	function SetTextureOffset(self, offset: number?): nil
end

declare class BindableEvent extends Instance
	Event: RBXScriptSignal<...any>
	function Fire(self, ...: any): ()
end

declare class BindableFunction extends Instance
	OnInvoke: (...any) -> ...any
	function Invoke(self, ...: any): ...any
end

declare class BodyMover extends Instance
end

declare class BodyAngularVelocity extends BodyMover
	AngularVelocity: Vector3
	MaxTorque: Vector3
	P: number
end

declare class BodyForce extends BodyMover
	Force: Vector3
end

declare class BodyGyro extends BodyMover
	CFrame: CFrame
	D: number
	MaxTorque: Vector3
	P: number
end

declare class BodyPosition extends BodyMover
	D: number
	MaxForce: Vector3
	P: number
	Position: Vector3
	ReachedTarget: RBXScriptSignal<>
	function GetLastForce(self): Vector3
end

declare class BodyThrust extends BodyMover
	Force: Vector3
	Location: Vector3
end

declare class BodyVelocity extends BodyMover
	MaxForce: Vector3
	P: number
	Velocity: Vector3
	function GetLastForce(self): Vector3
	function lastForce(self): Vector3
end



declare class Breakpoint extends Instance
	Condition: string
	ContinueExecution: boolean
	Enabled: boolean
	Id: number
	Line: number
	LogMessage: string
	MetaBreakpointId: number
	RemoveOnHit: boolean
	Script: string
	Valid: boolean
	Verified: boolean
end

declare class BrowserService extends Instance
	AuthCookieCopiedToEngine: RBXScriptSignal<>
	BrowserWindowClosed: RBXScriptSignal<>
	BrowserWindowWillNavigate: RBXScriptSignal<string>
	JavaScriptCallback: RBXScriptSignal<string>
	function CloseBrowserWindow(self): nil
	function CopyAuthCookieFromBrowserToEngine(self): nil
	function EmitHybridEvent(self, moduleName: string, eventName: string, params: string): nil
	function ExecuteJavaScript(self, javascript: string): nil
	function OpenBrowserWindow(self, url: string): nil
	function OpenNativeOverlay(self, title: string, url: string): nil
	function OpenWeChatAuthWindow(self): nil
	function ReturnToJavaScript(self, callbackId: string, success: boolean, params: string): nil
	function SendCommand(self, command: string): nil
end

declare class BubbleChatMessageProperties extends Instance
	BackgroundColor3: Color3
	BackgroundTransparency: number
	FontFace: Font
	TextColor3: Color3
	TextSize: number
end

declare class BulkImportService extends Instance
	AssetImported: RBXScriptSignal<EnumAssetType, string, number>
	BulkImportFinished: RBXScriptSignal<number>
	BulkImportStarted: RBXScriptSignal<>
	function LaunchBulkImport(self, assetTypeToImport: number): nil
	function ShowBulkImportView(self): nil
end

declare class CacheableContentProvider extends Instance
end

declare class HSRDataContentProvider extends CacheableContentProvider
end

declare class MeshContentProvider extends CacheableContentProvider
	function GetContentMemoryData(self): { [any]: any }
end

declare class SolidModelContentProvider extends CacheableContentProvider
end

declare class CalloutService extends Instance
	function AttachCallout(self, definitionId: string, locationId: string, target: Instance): nil
	function DefineCallout(self, definitionId: string, title: string, description: string, learnMoreURL: string): nil
	function DetachCalloutsByDefinitionId(self, definitionId: string): nil
end

declare class Camera extends Instance
	CFrame: CFrame
	CameraSubject: Humanoid | BasePart | nil
	CameraType: EnumCameraType
	DiagonalFieldOfView: number
	FieldOfView: number
	FieldOfViewMode: EnumFieldOfViewMode
	FirstPersonTransition: RBXScriptSignal<boolean>
	Focus: CFrame
	HeadLocked: boolean
	HeadScale: number
	InterpolationFinished: RBXScriptSignal<>
	MaxAxisFieldOfView: number
	NearPlaneZ: number
	VRTiltAndRollEnabled: boolean
	ViewportSize: Vector2
	function GetPartsObscuringTarget(self, castPoints: { Vector3 }, ignoreList: { Instance }): { BasePart }
	function GetRenderCFrame(self): CFrame
	function GetRoll(self): number
	function ScreenPointToRay(self, x: number, y: number, depth: number?): Ray
	function SetCameraPanMode(self, mode: EnumCameraPanMode?): nil
	function SetImageServerView(self, modelCoord: CFrame): nil
	function SetRoll(self, rollAngle: number): nil
	function ViewportPointToRay(self, x: number, y: number, depth: number?): Ray
	function WorldToScreenPoint(self, worldPoint: Vector3): (Vector3, boolean)
	function WorldToViewportPoint(self, worldPoint: Vector3): (Vector3, boolean)
	function Zoom(self, distance: number): boolean
	function ZoomToExtents(self, boundingBoxCFrame: CFrame, boundingBoxSize: Vector3): nil
end

declare class CaptureService extends Instance
	CaptureSaved: RBXScriptSignal<{ [any]: any }>
	function GetCaptureSizeAsync(self, captureContentId: Content): Vector2
	function RetrieveCaptures(self): { any }
	function SaveScreenshotCapture(self): nil
end

declare class ChangeHistoryService extends Instance
	OnRecordingFinished: RBXScriptSignal<string, string?, string?, EnumFinishRecordingOperation, { [any]: any }?>
	OnRecordingStarted: RBXScriptSignal<string, string?>
	OnRedo: RBXScriptSignal<string>
	OnUndo: RBXScriptSignal<string>
	function FinishRecording(self, identifier: string, operation: EnumFinishRecordingOperation, finalOptions: { [any]: any }?): nil
	function GetCanRedo(self): any
	function GetCanUndo(self): any
	function IsRecordingInProgress(self, identifier: string?): boolean
	function Redo(self): nil
	function ResetWaypoints(self): nil
	function SetEnabled(self, state: boolean): nil
	function SetWaypoint(self, name: string): nil
	function TryBeginRecording(self, name: string, displayName: string?): string?
	function Undo(self): nil
end

declare class CharacterAppearance extends Instance
end

declare class BodyColors extends CharacterAppearance
	HeadColor3: Color3
	HeadColor: BrickColor
	LeftArmColor3: Color3
	LeftArmColor: BrickColor
	LeftLegColor3: Color3
	LeftLegColor: BrickColor
	RightArmColor3: Color3
	RightArmColor: BrickColor
	RightLegColor3: Color3
	RightLegColor: BrickColor
	TorsoColor3: Color3
	TorsoColor: BrickColor
end

declare class CharacterMesh extends CharacterAppearance
	BaseTextureId: number
	BodyPart: EnumBodyPart
	MeshId: number
	OverlayTextureId: number
end

declare class Clothing extends CharacterAppearance
	Color3: Color3
end

declare class Pants extends Clothing
	PantsTemplate: Content
end

declare class Shirt extends Clothing
	ShirtTemplate: Content
end

declare class ShirtGraphic extends CharacterAppearance
	Color3: Color3
	Graphic: Content
end



declare class Chat extends Instance
	BubbleChatEnabled: boolean
	BubbleChatSettingsChanged: RBXScriptSignal<any>
	Chatted: RBXScriptSignal<BasePart, string, EnumChatColor>
	LoadDefaultChat: boolean
	function CanUserChatAsync(self, userId: number): boolean
	function CanUsersChatAsync(self, userIdFrom: number, userIdTo: number): boolean
	function Chat(self, partOrCharacter: Instance, message: string, color: EnumChatColor?): nil
	function ChatLocal(self, partOrCharacter: Instance, message: string, color: EnumChatColor?): nil
	function FilterStringAsync(self, stringToFilter: string, playerFrom: Player, playerTo: Player): string
	function FilterStringForBroadcast(self, stringToFilter: string, playerFrom: Player): string
	function GetShouldUseLuaChat(self): boolean
	function InvokeChatCallback(self, callbackType: EnumChatCallbackType, callbackArguments: any): any
	function RegisterChatCallback(self, callbackType: EnumChatCallbackType, callbackFunction: ((...any) -> ...any)): nil
	function SetBubbleChatSettings(self, settings: any): nil
end

declare class ClickDetector extends Instance
	CursorIcon: Content
	MaxActivationDistance: number
	MouseClick: RBXScriptSignal<Player>
	MouseHoverEnter: RBXScriptSignal<Player>
	MouseHoverLeave: RBXScriptSignal<Player>
	RightMouseClick: RBXScriptSignal<Player>
end

declare class DragDetector extends ClickDetector
	ActivatedCursorIcon: Content
	ApplyAtCenterOfMass: boolean
	Axis: Vector3
	DragContinue: RBXScriptSignal<Player, Ray, CFrame, CFrame?, boolean>
	DragEnd: RBXScriptSignal<Player>
	DragFrame: CFrame
	DragStart: RBXScriptSignal<Player, Ray, CFrame, CFrame, BasePart, CFrame?, boolean>
	DragStyle: EnumDragDetectorDragStyle
	Enabled: boolean
	GamepadModeSwitchKeyCode: EnumKeyCode
	KeyboardModeSwitchKeyCode: EnumKeyCode
	MaxDragAngle: number
	MaxDragTranslation: Vector3
	MaxForce: number
	MaxTorque: number
	MinDragAngle: number
	MinDragTranslation: Vector3
	Orientation: Vector3
	ReferenceInstance: Instance
	ResponseStyle: EnumDragDetectorResponseStyle
	Responsiveness: number
	RunLocally: boolean
	SecondaryAxis: Vector3
	TrackballRadialPullFactor: number
	TrackballRollFactor: number
	VRSwitchKeyCode: EnumKeyCode
	WorldAxis: Vector3
	WorldSecondaryAxis: Vector3
	function AddConstraintFunction(self, priority: number, func: ((...any) -> ...any)): RBXScriptConnection
	function GetReferenceFrame(self): CFrame
	function RestartDrag(self): nil
	function SetDragStyleFunction(self, func: ((...any) -> ...any)): nil
end

declare class Clouds extends Instance
	Color: Color3
	Cover: number
	Density: number
	Enabled: boolean
end

declare class ClusterPacketCache extends Instance
end

declare class CollectionService extends Instance
	TagAdded: RBXScriptSignal<string>
	TagRemoved: RBXScriptSignal<string>
	function AddTag(self, instance: Instance, tag: string): nil
	function GetAllTags(self): { string }
	function GetInstanceAddedSignal(self, tag: string): RBXScriptSignal<Instance>
	function GetInstanceRemovedSignal(self, tag: string): RBXScriptSignal<Instance>
	function GetTagged(self, tag: string): { Instance }
	function GetTags(self, instance: Instance): { string }
	function HasTag(self, instance: Instance, tag: string): boolean
	function RemoveTag(self, instance: Instance, tag: string): nil
end

declare class CommandInstance extends Instance
	AllowGUIAccessPoints: boolean
	Checked: boolean
	DefaultShortcut: string
	DisplayName: string
	Enabled: boolean
	Icon: string
	Name: string
	Permission: EnumCommandPermission
	StatusTip: string
	function EnableGuiAccess(self, displayName: string, statusTip: string, defaultShortcut: string): nil
	function RegisterExecutionCallback(self, callbackFunction: ((...any) -> ...any)): nil
end

declare class CommandService extends Instance
	CommandExecuting: RBXScriptSignal<string, any>
	function Execute(self, name: string, params: any): any
	function RegisterCommand(self, plugin: Plugin, name: string, context: string, permission: EnumCommandPermission?): CommandInstance
end

declare class Configuration extends Instance
end

declare class ConfigureServerService extends Instance
end

declare class Constraint extends Instance
	Active: boolean
	Attachment0: Attachment
	Attachment1: Attachment
	Color: BrickColor
	Enabled: boolean
	Visible: boolean
end

declare class AlignOrientation extends Constraint
	AlignType: EnumAlignType
	CFrame: CFrame
	MaxAngularVelocity: number
	MaxTorque: number
	Mode: EnumOrientationAlignmentMode
	PrimaryAxis: Vector3
	PrimaryAxisOnly: boolean
	ReactionTorqueEnabled: boolean
	Responsiveness: number
	RigidityEnabled: boolean
	SecondaryAxis: Vector3
end

declare class AlignPosition extends Constraint
	ApplyAtCenterOfMass: boolean
	ForceLimitMode: EnumForceLimitMode
	ForceRelativeTo: EnumActuatorRelativeTo
	MaxAxesForce: Vector3
	MaxForce: number
	MaxVelocity: number
	Mode: EnumPositionAlignmentMode
	Position: Vector3
	ReactionForceEnabled: boolean
	Responsiveness: number
	RigidityEnabled: boolean
end

declare class AngularVelocity extends Constraint
	AngularVelocity: Vector3
	MaxTorque: number
	ReactionTorqueEnabled: boolean
	RelativeTo: EnumActuatorRelativeTo
end

declare class AnimationConstraint extends Constraint
	IsKinematic: boolean
	MaxForce: number
	MaxTorque: number
	Transform: CFrame
end

declare class BallSocketConstraint extends Constraint
	LimitsEnabled: boolean
	MaxFrictionTorque: number
	Radius: number
	Restitution: number
	TwistLimitsEnabled: boolean
	TwistLowerAngle: number
	TwistUpperAngle: number
	UpperAngle: number
end

declare class HingeConstraint extends Constraint
	ActuatorType: EnumActuatorType
	AngularResponsiveness: number
	AngularSpeed: number
	AngularVelocity: number
	CurrentAngle: number
	LimitsEnabled: boolean
	LowerAngle: number
	MotorMaxAcceleration: number
	MotorMaxTorque: number
	Radius: number
	Restitution: number
	ServoMaxTorque: number
	TargetAngle: number
	UpperAngle: number
end

declare class LineForce extends Constraint
	ApplyAtCenterOfMass: boolean
	InverseSquareLaw: boolean
	Magnitude: number
	MaxForce: number
	ReactionForceEnabled: boolean
end

declare class LinearVelocity extends Constraint
	ForceLimitMode: EnumForceLimitMode
	LineDirection: Vector3
	LineVelocity: number
	MaxAxesForce: Vector3
	MaxForce: number
	MaxPlanarAxesForce: Vector2
	PlaneVelocity: Vector2
	PrimaryTangentAxis: Vector3
	RelativeTo: EnumActuatorRelativeTo
	SecondaryTangentAxis: Vector3
	VectorVelocity: Vector3
	VelocityConstraintMode: EnumVelocityConstraintMode
end

declare class PlaneConstraint extends Constraint
end



declare class RigidConstraint extends Constraint
end

declare class RodConstraint extends Constraint
	CurrentDistance: number
	Length: number
	LimitAngle0: number
	LimitAngle1: number
	LimitsEnabled: boolean
	Thickness: number
end

declare class RopeConstraint extends Constraint
	CurrentDistance: number
	Length: number
	Restitution: number
	Thickness: number
	WinchEnabled: boolean
	WinchForce: number
	WinchResponsiveness: number
	WinchSpeed: number
	WinchTarget: number
end

declare class SlidingBallConstraint extends Constraint
	ActuatorType: EnumActuatorType
	CurrentPosition: number
	LimitsEnabled: boolean
	LinearResponsiveness: number
	LowerLimit: number
	MotorMaxAcceleration: number
	MotorMaxForce: number
	Restitution: number
	ServoMaxForce: number
	Size: number
	Speed: number
	TargetPosition: number
	UpperLimit: number
	Velocity: number
end

declare class CylindricalConstraint extends SlidingBallConstraint
	AngularActuatorType: EnumActuatorType
	AngularLimitsEnabled: boolean
	AngularResponsiveness: number
	AngularRestitution: number
	AngularSpeed: number
	AngularVelocity: number
	CurrentAngle: number
	InclinationAngle: number
	LowerAngle: number
	MotorMaxAngularAcceleration: number
	MotorMaxTorque: number
	RotationAxisVisible: boolean
	ServoMaxTorque: number
	TargetAngle: number
	UpperAngle: number
	WorldRotationAxis: Vector3
end

declare class PrismaticConstraint extends SlidingBallConstraint
end

declare class SpringConstraint extends Constraint
	Coils: number
	CurrentLength: number
	Damping: number
	FreeLength: number
	LimitsEnabled: boolean
	MaxForce: number
	MaxLength: number
	MinLength: number
	Radius: number
	Stiffness: number
	Thickness: number
end

declare class Torque extends Constraint
	RelativeTo: EnumActuatorRelativeTo
	Torque: Vector3
end

declare class TorsionSpringConstraint extends Constraint
	Coils: number
	CurrentAngle: number
	Damping: number
	LimitsEnabled: boolean
	MaxAngle: number
	MaxTorque: number
	Radius: number
	Restitution: number
	Stiffness: number
end

declare class UniversalConstraint extends Constraint
	LimitsEnabled: boolean
	MaxAngle: number
	Radius: number
	Restitution: number
end

declare class VectorForce extends Constraint
	ApplyAtCenterOfMass: boolean
	Force: Vector3
	RelativeTo: EnumActuatorRelativeTo
end

declare class ContentProvider extends Instance
	AssetFetchFailed: RBXScriptSignal<Content>
	BaseUrl: string
	RequestQueueSize: number
	function CalculateNumTrianglesInMesh(self, meshId: string): number
	function CalculateNumTrianglesInMeshSync(self, meshId: string): number
	function GetAssetFetchStatus(self, contentId: Content): EnumAssetFetchStatus
	function GetAssetFetchStatusChangedSignal(self, contentId: Content): RBXScriptSignal
	function GetDetailedFailedRequests(self): { any }
	function GetFailedRequests(self): { any }
	function ListEncryptedAssets(self): { any }
	function PreloadAsync(self, contentIdList: { any }, callbackFunction: ((...any) -> ...any)?): nil
	function RegisterDefaultEncryptionKey(self, encryptionKey: string): nil
	function RegisterDefaultSessionKey(self, sessionKey: string): nil
	function RegisterEncryptedAsset(self, assetId: Content, encryptionKey: string): nil
	function RegisterSessionEncryptedAsset(self, contentId: Content, sessionKey: string): nil
	function SetBaseUrl(self, url: string): nil
	function UnregisterDefaultEncryptionKey(self): nil
	function UnregisterEncryptedAsset(self, assetId: Content): nil
end

declare class ContextActionService extends Instance
	BoundActionAdded: RBXScriptSignal<string, boolean, { [any]: any }, boolean>
	BoundActionChanged: RBXScriptSignal<string, string, { [any]: any }>
	BoundActionRemoved: RBXScriptSignal<string, { [any]: any }, boolean>
	GetActionButtonEvent: RBXScriptSignal<string>
	LocalToolEquipped: RBXScriptSignal<Tool>
	LocalToolUnequipped: RBXScriptSignal<Tool>
	function BindAction(self, actionName: string, functionToBind: (actionName: string, inputState: EnumUserInputState, inputObject: InputObject) -> EnumContextActionResult?, createTouchButton: boolean, ...: EnumUserInputType | EnumKeyCode): ()
	function BindActionAtPriority(self, actionName: string, functionToBind: (actionName: string, inputState: EnumUserInputState, inputObject: InputObject) -> EnumContextActionResult?, createTouchButton: boolean, priorityLevel: number, ...: EnumUserInputType | EnumKeyCode): ()
	function BindActivate(self, userInputTypeForActivation: EnumUserInputType, keyCodesForActivation: any): nil
	function BindCoreAction(self, actionName: string, functionToBind: ((...any) -> ...any), createTouchButton: boolean, inputTypes: any): nil
	function BindCoreActionAtPriority(self, actionName: string, functionToBind: ((...any) -> ...any), createTouchButton: boolean, priorityLevel: number, inputTypes: any): nil
	function BindCoreActivate(self, userInputTypeForActivation: EnumUserInputType, keyCodesForActivation: any): nil
	function CallFunction(self, actionName: string, state: EnumUserInputState, inputObject: Instance): any
	function FireActionButtonFoundSignal(self, actionName: string, actionButton: Instance): nil
	function GetAllBoundActionInfo(self): { [any]: any }
	function GetAllBoundCoreActionInfo(self): { [any]: any }
	function GetBoundActionInfo(self, actionName: string): { [any]: any }
	function GetBoundCoreActionInfo(self, actionName: string): { [any]: any }
	function GetButton(self, actionName: string): ImageButton
	function GetCurrentLocalToolIcon(self): string
	function SetDescription(self, actionName: string, description: string): nil
	function SetImage(self, actionName: string, image: string): nil
	function SetPosition(self, actionName: string, position: UDim2): nil
	function SetTitle(self, actionName: string, title: string): nil
	function UnbindAction(self, actionName: string): nil
	function UnbindActivate(self, userInputTypeForActivation: EnumUserInputType, keyCodeForActivation: EnumKeyCode?): nil
	function UnbindAllActions(self): nil
	function UnbindCoreAction(self, actionName: string): nil
	function UnbindCoreActivate(self, userInputTypeForActivation: EnumUserInputType, keyCodeForActivation: EnumKeyCode?): nil
end

declare class Controller extends Instance
	ButtonChanged: RBXScriptSignal<EnumButton>
	function BindButton(self, button: EnumButton, caption: string): nil
	function GetButton(self, button: EnumButton): boolean
	function UnbindButton(self, button: EnumButton): nil
end

declare class HumanoidController extends Controller
end

declare class SkateboardController extends Controller
	AxisChanged: RBXScriptSignal<string>
	Steer: number
	Throttle: number
end

declare class VehicleController extends Controller
end

declare class ControllerBase extends Instance
	Active: boolean
	BalanceRigidityEnabled: boolean
	MoveSpeedFactor: number
end

declare class AirController extends ControllerBase
	BalanceMaxTorque: number
	BalanceSpeed: number
	LinearImpulse: Vector3
	MaintainAngularMomentum: boolean
	MaintainLinearMomentum: boolean
	MoveMaxForce: number
	TurnMaxTorque: number
	TurnSpeedFactor: number
end

declare class ClimbController extends ControllerBase
	AccelerationTime: number
	BalanceMaxTorque: number
	BalanceSpeed: number
	MoveMaxForce: number
end

declare class GroundController extends ControllerBase
	AccelerationLean: number
	AccelerationTime: number
	BalanceMaxTorque: number
	BalanceSpeed: number
	DecelerationTime: number
	Friction: number
	FrictionWeight: number
	GroundOffset: number
	StandForce: number
	StandSpeed: number
	TurnSpeedFactor: number
end

declare class SwimController extends ControllerBase
	AccelerationTime: number
	PitchMaxTorque: number
	PitchSpeedFactor: number
	RollMaxTorque: number
	RollSpeedFactor: number
end

declare class ControllerManager extends Instance
	ActiveController: ControllerBase
	BaseMoveSpeed: number
	BaseTurnSpeed: number
	ClimbSensor: ControllerSensor
	FacingDirection: Vector3
	GroundSensor: ControllerSensor
	MovingDirection: Vector3
	RootPart: BasePart
end

declare class ControllerService extends Instance
end

declare class CookiesService extends Instance
end

declare class CorePackages extends Instance
end

declare class CoreScriptDebuggingManagerHelper extends Instance
end

declare class CoreScriptSyncService extends Instance
	function GetScriptFilePath(self, script: Instance): any
end

declare class CrossDMScriptChangeListener extends Instance
	GuidLineContentsChanged: RBXScriptSignal<string, number, string>
	GuidNameChanged: RBXScriptSignal<string, string>
	function IsWatchingScriptLine(self, scriptRef: string, lineNumber: number): boolean
	function StartWatchingScriptLine(self, scriptRef: string, debuggerConnectionId: number, lineNumber: number): nil
end





declare class DataModelMesh extends Instance
	Offset: Vector3
	Scale: Vector3
	VertexColor: Vector3
end





declare class DynamicMesh extends DataModelMesh
	function AddTriangle(self, vertexId0: number, vertexId1: number, vertexId2: number): number
	function AddVertex(self, p: Vector3): number
	function CreateMeshPartAsync(self, collisionFidelity: EnumCollisionFidelity): MeshPart
	function FindClosestPointOnSurface(self, point: Vector3): any
	function FindClosestVertex(self, toThisPoint: Vector3): number
	function FindVerticesWithinSphere(self, center: Vector3, radius: number): { any }
	function GetAdjacentTriangles(self, triangleId: number): { any }
	function GetAdjacentVertices(self, vertexId: number): { any }
	function GetPosition(self, vertexId: number): Vector3
	function GetTriangleVertices(self, triangleId: number): any
	function GetTriangles(self): { any }
	function GetUV(self, vertexId: number): Vector2
	function GetVertexColor(self, vertexId: number): Color3
	function GetVertexColorAlpha(self, vertexId: number): number
	function GetVertexNormal(self, vertexId: number): Vector3
	function GetVertices(self): { any }
	function InitializeFromMeshIdAsync(self, meshId: Content): nil
	function InitializeFromMeshPartAsync(self, meshPart: Instance): nil
	function Raycast(self, origin: Vector3, direction: Vector3): any
	function RemoveTriangle(self, triangleId: number): nil
	function RemoveVertex(self, vertexId: number): nil
	function SetPosition(self, vertexId: number, p: Vector3): nil
	function SetUV(self, vertexId: number, uv: Vector2): nil
	function SetVertexColor(self, vertexId: number, color: Color3): nil
	function SetVertexColorAlpha(self, vertexId: number, alpha: number): nil
	function SetVertexNormal(self, vertexId: number, vnormal: Vector3): nil
end

declare class FileMesh extends DataModelMesh
	MeshId: Content
	TextureId: Content
end

declare class SpecialMesh extends FileMesh
	MeshType: EnumMeshType
end

declare class DataModelPatchService extends Instance
	function GetPatch(self, patchName: string): Instance
	function RegisterPatch(self, patchName: string, behaviorName: string, localConfigPath: string, userId: number): nil
	function UpdatePatch(self, userId: number, patchName: string, callbackFunction: ((...any) -> ...any)): nil
end

declare class DataModelSession extends Instance
	CurrentDataModelType: EnumStudioDataModelType
	CurrentDataModelTypeAboutToChange: RBXScriptSignal<EnumStudioDataModelType>
	CurrentDataModelTypeChanged: RBXScriptSignal<>
	DataModelCreated: RBXScriptSignal<EnumStudioDataModelType>
	DataModelWillBeDestroyed: RBXScriptSignal<EnumStudioDataModelType>
	SessionId: string
end

declare class DataStoreIncrementOptions extends Instance
	function GetMetadata(self): { [any]: any }
	function SetMetadata(self, attributes: { [any]: any }): nil
end

declare class DataStoreInfo extends Instance
	CreatedTime: number
	DataStoreName: string
	UpdatedTime: number
end

declare class DataStoreKey extends Instance
	KeyName: string
end

declare class DataStoreKeyInfo extends Instance
	CreatedTime: number
	UpdatedTime: number
	Version: string
	function GetMetadata(self): { [any]: any }
	function GetUserIds(self): { number }
end

declare class DataStoreObjectVersionInfo extends Instance
	CreatedTime: number
	IsDeleted: boolean
	Version: string
end

declare class DataStoreOptions extends Instance
	AllScopes: boolean
	function SetExperimentalFeatures(self, experimentalFeatures: { [any]: any }): nil
end

declare class DataStoreService extends Instance
	AutomaticRetry: boolean
	function GetDataStore(self, name: string, scope: string?, options: Instance?): DataStore
	function GetGlobalDataStore(self): GlobalDataStore
	function GetOrderedDataStore(self, name: string, scope: string?): OrderedDataStore
	function GetRequestBudgetForRequestType(self, requestType: EnumDataStoreRequestType): number
	function ListDataStoresAsync(self, prefix: string?, pageSize: number?, cursor: string?): DataStoreListingPages
end

declare class DataStoreSetOptions extends Instance
	function GetMetadata(self): { [any]: any }
	function SetMetadata(self, attributes: { [any]: any }): nil
end

declare class Debris extends Instance
	function AddItem(self, item: Instance, lifetime: number?): nil
	function SetLegacyMaxItems(self, enabled: boolean): nil
end

declare class DebugSettings extends Instance
	DataModel: number
	InstanceCount: number
	IsScriptStackTracingEnabled: boolean
	JobCount: number
	PlayerCount: number
	ReportSoundWarnings: boolean
	RobloxVersion: string
	TickCountPreciseOverride: EnumTickCountSampleMethod
end

declare class DebuggablePluginWatcher extends Instance
end

declare class DebuggerBreakpoint extends Instance
	Condition: string
	ContinueExecution: boolean
	IsEnabled: boolean
	Line: number
	LogExpression: string
	isContextDependentBreakpoint: boolean
end

declare class DebuggerConnection extends Instance
	BreakpointAdded: RBXScriptSignal<Breakpoint>
	BreakpointChanged: RBXScriptSignal<Breakpoint>
	BreakpointRemoved: RBXScriptSignal<Breakpoint, EnumBreakpointRemoveReason>
	ErrorMessage: string
	HasError: boolean
	Id: number
	IsPaused: boolean
	Paused: RBXScriptSignal<PausedState, EnumDebuggerPauseReason>
	Resumed: RBXScriptSignal<PausedState>
	function AddBreakpoint(self, script: string, line: number, breakpoint: Breakpoint): nil
	function Close(self): nil
	function EvaluateWatch(self, expression: string, frame: StackFrame, callback: ((...any) -> ...any)): number
	function GetFrameById(self, id: number): StackFrame
	function GetSource(self, scriptRef: string, status: ((...any) -> ...any)): number
	function GetThreadById(self, id: number): ThreadState
	function GetThreads(self, callback: ((...any) -> ...any)): number
	function GetVariableById(self, id: number): DebuggerVariable
	function Pause(self, thread: ThreadState, status: ((...any) -> ...any)): number
	function Populate(self, instance: Instance, callback: ((...any) -> ...any)): number
	function RemoveBreakpoint(self, breakpoint: Breakpoint): nil
	function Resume(self, thread: ThreadState, status: ((...any) -> ...any)): number
	function SetExceptionBreakMode(self, breakMode: EnumDebuggerExceptionBreakMode, callback: ((...any) -> ...any)): number
	function SetVariable(self, variable: DebuggerVariable, value: string, callback: ((...any) -> ...any)): number
	function Step(self, thread: ThreadState, callback: ((...any) -> ...any)): number
	function StepIn(self, thread: ThreadState, callback: ((...any) -> ...any)): number
	function StepOut(self, thread: ThreadState, callback: ((...any) -> ...any)): number
	function UpdateSelectedFrame(self, threadId: number, frameNumber: number): nil
end

declare class LocalDebuggerConnection extends DebuggerConnection
end

declare class DebuggerConnectionManager extends Instance
	ConnectionEnded: RBXScriptSignal<DebuggerConnection, EnumDebuggerEndReason>
	ConnectionStarted: RBXScriptSignal<DebuggerConnection>
	FocusChanged: RBXScriptSignal<DebuggerConnection>
	Timeout: number
	function ConnectLocal(self, dataModel: DataModel): number
	function ConnectRemote(self, host: string, port: number): number
	function FocusConnection(self, connection: DebuggerConnection): nil
	function GetConnectionById(self, id: number): DebuggerConnection
end

declare class DebuggerLuaResponse extends Instance
	IsError: boolean
	IsSuccess: boolean
	Message: string
	RequestId: number
	Status: EnumDebuggerStatus
	function GetArg(self): any
end

declare class DebuggerManager extends Instance
	DebuggerAdded: RBXScriptSignal<Instance>
	DebuggerRemoved: RBXScriptSignal<Instance>
	DebuggingEnabled: boolean
	function AddDebugger(self, script: Instance): Instance
	function EnableDebugging(self): nil
	function GetDebuggers(self): { Instance }
	function Resume(self): nil
end

declare class DebuggerUIService extends Instance
	ExpressionAdded: RBXScriptSignal<string>
	ExpressionsCleared: RBXScriptSignal<>
	function EditBreakpoint(self, metaBreakpointId: number): nil
	function EditWatch(self, expression: string): nil
	function IsConnectionForPlayDataModel(self, debuggerConnectionId: number): boolean
	function OpenScriptAtLine(self, guid: string, debuggerConnectionId: number, line: number, showErrorOnFail: boolean): nil
	function Pause(self): nil
	function RemoveScriptLineMarkers(self, debuggerConnectionId: number, allMarkers: boolean): nil
	function Resume(self): nil
	function SetCurrentThreadId(self, debuggerThreadId: number): nil
	function SetScriptLineMarker(self, guid: string, debuggerConnectionId: number, line: number, lineMarkerType: boolean): nil
	function SetWatchExpressions(self, expressions: { any }): nil
end

declare class DebuggerVariable extends Instance
	Name: string
	Populated: boolean
	Type: string
	Value: string
	VariableId: number
	VariablesCount: number
	function GetVariableByIndex(self, index: number): DebuggerVariable
	function GetVariableByName(self, name: string): DebuggerVariable
end

declare class DebuggerWatch extends Instance
	Expression: string
end

declare class DeviceIdService extends Instance
	function GetDeviceId(self): string
end

declare class Dialog extends Instance
	BehaviorType: EnumDialogBehaviorType
	ConversationDistance: number
	DialogChoiceSelected: RBXScriptSignal<Player, DialogChoice>
	GoodbyeChoiceActive: boolean
	GoodbyeDialog: string
	InUse: boolean
	InitialPrompt: string
	Purpose: EnumDialogPurpose
	Tone: EnumDialogTone
	TriggerDistance: number
	TriggerOffset: Vector3
	function GetCurrentPlayers(self): { Player }
	function SetPlayerIsUsing(self, player: Instance, isUsing: boolean): nil
	function SignalDialogChoiceSelected(self, player: Instance, dialogChoice: Instance): nil
end

declare class DialogChoice extends Instance
	GoodbyeChoiceActive: boolean
	GoodbyeDialog: string
	ResponseDialog: string
	UserDialog: string
end

declare class DraftsService extends Instance
	CommitStatusChanged: RBXScriptSignal<Instance, EnumDraftStatusCode>
	DraftAdded: RBXScriptSignal<Instance>
	DraftRemoved: RBXScriptSignal<Instance>
	DraftStatusChanged: RBXScriptSignal<Instance>
	EditorsListChanged: RBXScriptSignal<Instance>
	UpdateStatusChanged: RBXScriptSignal<Instance, EnumDraftStatusCode>
	function CommitEdits(self, scripts: { Instance }): nil
	function DiscardEdits(self, scripts: { Instance }): nil
	function GetDraftStatus(self, script: Instance): EnumDraftStatusCode
	function GetDrafts(self): { Instance }
	function GetEditors(self, script: Instance): { Instance }
	function RestoreScripts(self, scripts: { Instance }): nil
	function ShowDiffsAgainstBase(self, scripts: { Instance }): nil
	function ShowDiffsAgainstServer(self, scripts: { Instance }): nil
	function UpdateToLatestVersion(self, scripts: { Instance }): nil
end

declare class Dragger extends Instance
	function AxisRotate(self, axis: EnumAxis?): nil
	function MouseDown(self, mousePart: Instance, pointOnMousePart: Vector3, parts: { Instance }): nil
	function MouseMove(self, mouseRay: Ray): nil
	function MouseUp(self): nil
end

declare class DraggerService extends Instance
	AlignDraggedObjects: boolean
	AngleSnapEnabled: boolean
	AngleSnapIncrement: number
	AnimateHover: boolean
	CollisionsEnabled: boolean
	DraggerCoordinateSpace: EnumDraggerCoordinateSpace
	DraggerMovementMode: EnumDraggerMovementMode
	GeometrySnapColor: Color3
	HoverAnimateFrequency: number
	HoverLineThickness: number
	HoverThickness: number
	JointsEnabled: boolean
	LinearSnapEnabled: boolean
	LinearSnapIncrement: number
	PivotSnapToGeometry: boolean
	ShowHover: boolean
	ShowPivotIndicator: boolean
end

declare class DynamicImage extends Instance
	Size: Vector2
	function Clear(self): nil
	function DrawCircle(self, center: Vector2, radius: number, color: Color3, alpha: number): nil
	function ReadPixels(self, position: Vector2, size: Vector2): { any }
	function Resize(self, newSize: Vector2): nil
	function Rotate(self, degrees: number, resizeCanvas: boolean?): nil
	function WritePixels(self, position: Vector2, size: Vector2, pixels: { any }): nil
end

declare class EulerRotationCurve extends Instance
	RotationOrder: EnumRotationOrder
	function GetAnglesAtTime(self, time: number): { any }
	function GetRotationAtTime(self, time: number): CFrame
	function X(self): FloatCurve
	function Y(self): FloatCurve
	function Z(self): FloatCurve
end

declare class EventIngestService extends Instance
	function SendEventDeferred(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function SendEventImmediately(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function SetRBXEvent(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function SetRBXEventStream(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
end

declare class ExperienceAuthService extends Instance
	OpenAuthPrompt: RBXScriptSignal<string, { any }, { [any]: any }>
	function ScopeCheckUIComplete(self, guid: string, scopes: { any }, result: EnumScopeCheckResult, metadata: { [any]: any }): nil
end

declare class ExperienceInviteOptions extends Instance
	InviteMessageId: string
	InviteUser: number
	LaunchData: string
	PromptMessage: string
end

declare class Explosion extends Instance
	BlastPressure: number
	BlastRadius: number
	DestroyJointRadiusPercent: number
	ExplosionType: EnumExplosionType
	Hit: RBXScriptSignal<BasePart, number>
	Position: Vector3
	TimeScale: number
	Visible: boolean
end

declare class FaceAnimatorService extends Instance
	AudioAnimationEnabled: boolean
	FlipHeadOrientation: boolean
	TrackerError: RBXScriptSignal<EnumTrackerError>
	TrackerPrompt: RBXScriptSignal<EnumTrackerPromptEvent>
	VideoAnimationEnabled: boolean
	function GetTrackerLodController(self): TrackerLodController
	function Init(self, videoEnabled: boolean, audioEnabled: boolean): nil
	function IsStarted(self): boolean
	function Start(self): nil
	function Step(self): nil
	function Stop(self): nil
end

declare class FaceControls extends Instance
	ChinRaiser: number
	ChinRaiserUpperLip: number
	Corrugator: number
	EyesLookDown: number
	EyesLookLeft: number
	EyesLookRight: number
	EyesLookUp: number
	FlatPucker: number
	Funneler: number
	JawDrop: number
	JawLeft: number
	JawRight: number
	LeftBrowLowerer: number
	LeftCheekPuff: number
	LeftCheekRaiser: number
	LeftDimpler: number
	LeftEyeClosed: number
	LeftEyeUpperLidRaiser: number
	LeftInnerBrowRaiser: number
	LeftLipCornerDown: number
	LeftLipCornerPuller: number
	LeftLipStretcher: number
	LeftLowerLipDepressor: number
	LeftNoseWrinkler: number
	LeftOuterBrowRaiser: number
	LeftUpperLipRaiser: number
	LipPresser: number
	LipsTogether: number
	LowerLipSuck: number
	MouthLeft: number
	MouthRight: number
	Pucker: number
	RightBrowLowerer: number
	RightCheekPuff: number
	RightCheekRaiser: number
	RightDimpler: number
	RightEyeClosed: number
	RightEyeUpperLidRaiser: number
	RightInnerBrowRaiser: number
	RightLipCornerDown: number
	RightLipCornerPuller: number
	RightLipStretcher: number
	RightLowerLipDepressor: number
	RightNoseWrinkler: number
	RightOuterBrowRaiser: number
	RightUpperLipRaiser: number
	TongueDown: number
	TongueOut: number
	TongueUp: number
	UpperLipSuck: number
end

declare class FaceInstance extends Instance
	Face: EnumNormalId
end

declare class Decal extends FaceInstance
	Color3: Color3
	LocalTransparencyModifier: number
	Texture: Content
	Transparency: number
	ZIndex: number
end

declare class Texture extends Decal
	OffsetStudsU: number
	OffsetStudsV: number
	StudsPerTileU: number
	StudsPerTileV: number
end

declare class FacialAnimationRecordingService extends Instance
	BiometricDataConsent: boolean
	function CheckOrRequestCameraPermission(self): string
	function IsAgeRestricted(self): boolean
end

declare class FacialAnimationStreamingServiceStats extends Instance
	function Get(self, label: string): number
	function GetWithPlayerId(self, label: string, playerId: number): number
end

declare class FacialAnimationStreamingServiceV2 extends Instance
	ServiceState: number
	function GetStats(self): FacialAnimationStreamingServiceStats
	function IsAudioEnabled(self, mask: number): boolean
	function IsPlaceEnabled(self, mask: number): boolean
	function IsServerEnabled(self, mask: number): boolean
	function IsVideoEnabled(self, mask: number): boolean
	function ResolveStateForUser(self, userId: number): number
end

declare class FacialAnimationStreamingSubsessionStats extends Instance
end

declare class Feature extends Instance
	FaceId: EnumNormalId
	InOut: EnumInOut
	LeftRight: EnumLeftRight
	TopBottom: EnumTopBottom
end





declare class File extends Instance
	Size: number
	function GetBinaryContents(self): string
	function GetTemporaryId(self): Content
end

declare class Fire extends Instance
	Color: Color3
	Enabled: boolean
	Heat: number
	SecondaryColor: Color3
	Size: number
	TimeScale: number
	function FastForward(self, numFrames: number): nil
end

declare class FlagStandService extends Instance
end

declare class FloatCurve extends Instance
	Length: number
	function GetKeyAtIndex(self, index: number): FloatCurveKey
	function GetKeyIndicesAtTime(self, time: number): { any }
	function GetKeys(self): { any }
	function GetValueAtTime(self, time: number): number?
	function InsertKey(self, key: FloatCurveKey): { any }
	function RemoveKeyAtIndex(self, startingIndex: number, count: number?): number
	function SetKeys(self, keys: { any }): number
end

declare class FlyweightService extends Instance
end

declare class CSGDictionaryService extends FlyweightService
end

declare class NonReplicatedCSGDictionaryService extends FlyweightService
end

declare class Folder extends Instance
end

declare class ForceField extends Instance
	Visible: boolean
end

declare class FriendService extends Instance
	FriendsUpdated: RBXScriptSignal<{ any }>
	function GetPlatformFriends(self): { any }
end



declare class GamePassService extends Instance
end

declare class GameSettings extends Instance
	VideoCaptureEnabled: boolean
	VideoRecording: boolean
	VideoRecordingChangeRequest: RBXScriptSignal<boolean>
end

declare class GamepadService extends Instance
	GamepadCursorEnabled: boolean
	GamepadThumbstick1Changed: RBXScriptSignal<Vector2>
	function DisableGamepadCursor(self): nil
	function EnableGamepadCursor(self, guiObject: Instance): nil
	function GetGamepadCursorPosition(self): Vector2
	function SetGamepadCursorPosition(self, position: Vector2): nil
end

declare class Geometry extends Instance
end

declare class GeometryService extends Instance
	function CalculateConstraintsToPreserve(self, source: Instance, destination: { Instance }, options: any): { [any]: any }
	function IntersectAsync(self, part: Instance, parts: { Instance }, options: any): { Instance }
	function SubtractAsync(self, part: Instance, parts: { Instance }, options: any): { Instance }
	function UnionAsync(self, part: Instance, parts: { Instance }, options: any): { Instance }
end

declare class GetTextBoundsParams extends Instance
	Font: Font
	Size: number
	Text: string
	Width: number
end

declare class GlobalDataStore extends Instance
	function GetAsync(self, key: string): (any, DataStoreKeyInfo)
	function IncrementAsync(self, key: string, delta: number?, userIds: { number }?, options: DataStoreIncrementOptions?): (number, DataStoreKeyInfo)
	function RemoveAsync(self, key: string): (any, DataStoreKeyInfo)
	function SetAsync(self, key: string, value: any, userIds: { number }?, options: DataStoreSetOptions?): string
	function UpdateAsync(self, key: string, transformFunction: ((any, DataStoreKeyInfo) -> (any, { number }?, {}?))): (any, DataStoreKeyInfo)
end

declare class DataStore extends GlobalDataStore
	function GetVersionAsync(self, key: string, version: string): any
	function ListKeysAsync(self, prefix: string?, pageSize: number?, cursor: string?, excludeDeleted: boolean?): DataStoreKeyPages
	function ListVersionsAsync(self, key: string, sortDirection: EnumSortDirection?, minDate: number?, maxDate: number?, pageSize: number?): DataStoreVersionPages
	function RemoveVersionAsync(self, key: string, version: string): nil
end

declare class OrderedDataStore extends GlobalDataStore
	function GetAsync(self, key: string): (number?, DataStoreKeyInfo)
	function GetSortedAsync(self, ascending: boolean, pageSize: number, minValue: number, maxValue: number): DataStorePages
	function RemoveAsync(self, key: string): (number?, DataStoreKeyInfo)
	function SetAsync(self, key: string, value: number, userIds: { number }?, options: DataStoreSetOptions?): string
	function UpdateAsync(self, key: string, transformFunction: ((number?, DataStoreKeyInfo) -> (number, { number }?, {}?))): (number?, DataStoreKeyInfo)
end

declare class GoogleAnalyticsConfiguration extends Instance
end

declare class GroupService extends Instance
	function GetAlliesAsync(self, groupId: number): StandardPages
	function GetEnemiesAsync(self, groupId: number): StandardPages
	function GetGroupInfoAsync(self, groupId: number): any
	function GetGroupsAsync(self, userId: number): { any }
end

declare class GuiBase extends Instance
end

declare class GuiBase2d extends GuiBase
	AbsolutePosition: Vector2
	AbsoluteRotation: number
	AbsoluteSize: Vector2
	AutoLocalize: boolean
	ClippedRect: Rect
	IsNotOccluded: boolean
	RawRect2D: Rect
	RootLocalizationTable: LocalizationTable
	SelectionBehaviorDown: EnumSelectionBehavior
	SelectionBehaviorLeft: EnumSelectionBehavior
	SelectionBehaviorRight: EnumSelectionBehavior
	SelectionBehaviorUp: EnumSelectionBehavior
	SelectionChanged: RBXScriptSignal<boolean, GuiObject, GuiObject>
	SelectionGroup: boolean
	TotalGroupScale: number
end

declare class GuiObject extends GuiBase2d
	Active: boolean
	AnchorPoint: Vector2
	AutomaticSize: EnumAutomaticSize
	BackgroundColor3: Color3
	BackgroundTransparency: number
	BorderColor3: Color3
	BorderMode: EnumBorderMode
	BorderSizePixel: number
	ClipsDescendants: boolean
	InputBegan: RBXScriptSignal<InputObject>
	InputChanged: RBXScriptSignal<InputObject>
	InputEnded: RBXScriptSignal<InputObject>
	LayoutOrder: number
	MouseEnter: RBXScriptSignal<number, number>
	MouseLeave: RBXScriptSignal<number, number>
	MouseMoved: RBXScriptSignal<number, number>
	MouseWheelBackward: RBXScriptSignal<number, number>
	MouseWheelForward: RBXScriptSignal<number, number>
	NextSelectionDown: GuiObject
	NextSelectionLeft: GuiObject
	NextSelectionRight: GuiObject
	NextSelectionUp: GuiObject
	Position: UDim2
	Rotation: number
	Selectable: boolean
	SelectionGained: RBXScriptSignal<>
	SelectionImageObject: GuiObject
	SelectionLost: RBXScriptSignal<>
	SelectionOrder: number
	Size: UDim2
	SizeConstraint: EnumSizeConstraint
	TouchLongPress: RBXScriptSignal<{ Vector2 }, EnumUserInputState>
	TouchPan: RBXScriptSignal<{ Vector2 }, Vector2, Vector2, EnumUserInputState>
	TouchPinch: RBXScriptSignal<{ Vector2 }, number, number, EnumUserInputState>
	TouchRotate: RBXScriptSignal<{ Vector2 }, number, number, EnumUserInputState>
	TouchSwipe: RBXScriptSignal<EnumSwipeDirection, number>
	TouchTap: RBXScriptSignal<{ Vector2 }>
	Transparency: number
	Visible: boolean
	ZIndex: number
	function TweenPosition(self, endPosition: UDim2, easingDirection: EnumEasingDirection?, easingStyle: EnumEasingStyle?, time: number?, override: boolean?, callback: ((...any) -> ...any)?): boolean
	function TweenSize(self, endSize: UDim2, easingDirection: EnumEasingDirection?, easingStyle: EnumEasingStyle?, time: number?, override: boolean?, callback: ((...any) -> ...any)?): boolean
	function TweenSizeAndPosition(self, endSize: UDim2, endPosition: UDim2, easingDirection: EnumEasingDirection?, easingStyle: EnumEasingStyle?, time: number?, override: boolean?, callback: ((...any) -> ...any)?): boolean
end

declare class CanvasGroup extends GuiObject
	GroupColor3: Color3
	GroupTransparency: number
end

declare class Frame extends GuiObject
	Style: EnumFrameStyle
end

declare class GuiButton extends GuiObject
	Activated: RBXScriptSignal<InputObject, number>
	AutoButtonColor: boolean
	Modal: boolean
	MouseButton1Click: RBXScriptSignal<>
	MouseButton1Down: RBXScriptSignal<number, number>
	MouseButton1Up: RBXScriptSignal<number, number>
	MouseButton2Click: RBXScriptSignal<>
	MouseButton2Down: RBXScriptSignal<number, number>
	MouseButton2Up: RBXScriptSignal<number, number>
	Selected: boolean
	Style: EnumButtonStyle
end

declare class ImageButton extends GuiButton
	ContentImageSize: Vector2
	HoverImage: Content
	Image: Content
	ImageColor3: Color3
	ImageRectOffset: Vector2
	ImageRectSize: Vector2
	ImageTransparency: number
	IsLoaded: boolean
	PressedImage: Content
	ResampleMode: EnumResamplerMode
	ScaleType: EnumScaleType
	SliceCenter: Rect
	SliceScale: number
	TileSize: UDim2
	function SetEnableContentImageSizeChangedEvents(self, enabled: boolean): nil
end

declare class TextButton extends GuiButton
	ContentText: string
	Font: EnumFont
	FontFace: Font
	LineHeight: number
	LocalizedText: string
	MaxVisibleGraphemes: number
	RichText: boolean
	Text: string
	TextBounds: Vector2
	TextColor3: Color3
	TextDirection: EnumTextDirection
	TextFits: boolean
	TextScaled: boolean
	TextSize: number
	TextStrokeColor3: Color3
	TextStrokeTransparency: number
	TextTransparency: number
	TextTruncate: EnumTextTruncate
	TextWrapped: boolean
	TextXAlignment: EnumTextXAlignment
	TextYAlignment: EnumTextYAlignment
	function SetTextFromInput(self, text: string): nil
end

declare class GuiLabel extends GuiObject
end

declare class ImageLabel extends GuiLabel
	ContentImageSize: Vector2
	Image: Content
	ImageColor3: Color3
	ImageRectOffset: Vector2
	ImageRectSize: Vector2
	ImageTransparency: number
	IsLoaded: boolean
	ResampleMode: EnumResamplerMode
	ScaleType: EnumScaleType
	SliceCenter: Rect
	SliceScale: number
	TileSize: UDim2
	function SetEnableContentImageSizeChangedEvents(self, enabled: boolean): nil
end

declare class TextLabel extends GuiLabel
	ContentText: string
	Font: EnumFont
	FontFace: Font
	LineHeight: number
	LocalizedText: string
	MaxVisibleGraphemes: number
	RichText: boolean
	Text: string
	TextBounds: Vector2
	TextColor3: Color3
	TextDirection: EnumTextDirection
	TextFits: boolean
	TextScaled: boolean
	TextSize: number
	TextStrokeColor3: Color3
	TextStrokeTransparency: number
	TextTransparency: number
	TextTruncate: EnumTextTruncate
	TextWrapped: boolean
	TextXAlignment: EnumTextXAlignment
	TextYAlignment: EnumTextYAlignment
	function SetTextFromInput(self, text: string): nil
end

declare class ScrollingFrame extends GuiObject
	AbsoluteCanvasSize: Vector2
	AbsoluteWindowSize: Vector2
	AutomaticCanvasSize: EnumAutomaticSize
	BottomImage: Content
	CanvasPosition: Vector2
	CanvasSize: UDim2
	ElasticBehavior: EnumElasticBehavior
	HorizontalBarRect: Rect
	HorizontalScrollBarInset: EnumScrollBarInset
	MaxCanvasPosition: Vector2
	MidImage: Content
	ScrollBarImageColor3: Color3
	ScrollBarImageTransparency: number
	ScrollBarThickness: number
	ScrollVelocity: Vector2
	ScrollingDirection: EnumScrollingDirection
	ScrollingEnabled: boolean
	TopImage: Content
	VerticalBarRect: Rect
	VerticalScrollBarInset: EnumScrollBarInset
	VerticalScrollBarPosition: EnumVerticalScrollBarPosition
	function ClearInertialScrolling(self): nil
	function GetSampledInertialVelocity(self): Vector2
	function ScrollToTop(self): nil
end

declare class TextBox extends GuiObject
	ClearTextOnFocus: boolean
	ContentText: string
	CursorPosition: number
	FocusLost: RBXScriptSignal<boolean, InputObject>
	Focused: RBXScriptSignal<>
	Font: EnumFont
	FontFace: Font
	LineHeight: number
	ManualFocusRelease: boolean
	MaxVisibleGraphemes: number
	MultiLine: boolean
	OverlayNativeInput: boolean
	PlaceholderColor3: Color3
	PlaceholderText: string
	ReturnKeyType: EnumReturnKeyType
	ReturnPressedFromOnScreenKeyboard: RBXScriptSignal<>
	RichText: boolean
	SelectionStart: number
	ShowNativeInput: boolean
	Text: string
	TextBounds: Vector2
	TextColor3: Color3
	TextDirection: EnumTextDirection
	TextEditable: boolean
	TextFits: boolean
	TextInputType: EnumTextInputType
	TextScaled: boolean
	TextSize: number
	TextStrokeColor3: Color3
	TextStrokeTransparency: number
	TextTransparency: number
	TextTruncate: EnumTextTruncate
	TextWrapped: boolean
	TextXAlignment: EnumTextXAlignment
	TextYAlignment: EnumTextYAlignment
	function CaptureFocus(self): nil
	function IsFocused(self): boolean
	function ReleaseFocus(self, submitted: boolean?): nil
	function ResetKeyboardMode(self): nil
	function SetTextFromInput(self, text: string): nil
end

declare class VideoFrame extends GuiObject
	DidLoop: RBXScriptSignal<string>
	Ended: RBXScriptSignal<string>
	IsLoaded: boolean
	Loaded: RBXScriptSignal<string>
	Looped: boolean
	Paused: RBXScriptSignal<string>
	Played: RBXScriptSignal<string>
	Playing: boolean
	Resolution: Vector2
	TimeLength: number
	TimePosition: number
	Video: Content
	Volume: number
	function Pause(self): nil
	function Play(self): nil
end

declare class ViewportFrame extends GuiObject
	Ambient: Color3
	CurrentCamera: Camera
	ImageColor3: Color3
	ImageTransparency: number
	IsMirrored: boolean
	LightColor: Color3
	LightDirection: Vector3
end

declare class LayerCollector extends GuiBase2d
	Enabled: boolean
	ResetOnSpawn: boolean
	ZIndexBehavior: EnumZIndexBehavior
end

declare class BillboardGui extends LayerCollector
	Active: boolean
	Adornee: Instance
	AlwaysOnTop: boolean
	Brightness: number
	ClipsDescendants: boolean
	CurrentDistance: number
	DistanceLowerLimit: number
	DistanceStep: number
	DistanceUpperLimit: number
	ExtentsOffset: Vector3
	ExtentsOffsetWorldSpace: Vector3
	LightInfluence: number
	MaxDistance: number
	PlayerToHideFrom: Instance
	Size: UDim2
	SizeOffset: Vector2
	StudsOffset: Vector3
	StudsOffsetWorldSpace: Vector3
	function GetScreenSpaceBounds(self): any
end

declare class PluginGui extends LayerCollector
	PluginDragDropped: RBXScriptSignal<{ [any]: any }>
	PluginDragEntered: RBXScriptSignal<{ [any]: any }>
	PluginDragLeft: RBXScriptSignal<{ [any]: any }>
	PluginDragMoved: RBXScriptSignal<{ [any]: any }>
	Title: string
	WindowFocusReleased: RBXScriptSignal<>
	WindowFocused: RBXScriptSignal<>
	function BindToClose(self, func: ((...any) -> ...any)?): nil
	function GetRelativeMousePosition(self): Vector2
end

declare class DockWidgetPluginGui extends PluginGui
	HostWidgetWasRestored: boolean
	function RequestRaise(self): nil
end

declare class QWidgetPluginGui extends PluginGui
end

declare class ScreenGui extends LayerCollector
	ClipToDeviceSafeArea: boolean
	DisplayOrder: number
	IgnoreGuiInset: boolean
	OnTopOfCoreBlur: boolean
	SafeAreaCompatibility: EnumSafeAreaCompatibility
	ScreenInsets: EnumScreenInsets
end



declare class SurfaceGuiBase extends LayerCollector
	Active: boolean
	Adornee: Instance
	Face: EnumNormalId
end

declare class AdGui extends SurfaceGuiBase
	AdShape: EnumAdShape
	FallbackImage: Content
	Status: EnumAdUnitStatus
end

declare class SurfaceGui extends SurfaceGuiBase
	AlwaysOnTop: boolean
	Brightness: number
	CanvasSize: Vector2
	ClipsDescendants: boolean
	HorizontalCurvature: number
	LightInfluence: number
	PixelsPerStud: number
	Shape: EnumSurfaceGuiShape
	SizingMode: EnumSurfaceGuiSizingMode
	ToolPunchThroughDistance: number
	ZOffset: number
end

declare class GuiBase3d extends GuiBase
	Color3: Color3
	Transparency: number
	Visible: boolean
end



declare class InstanceAdornment extends GuiBase3d
	Adornee: Instance?
end

declare class SelectionBox extends InstanceAdornment
	LineThickness: number
	StudioSelectionBox: boolean
	SurfaceColor3: Color3
	SurfaceTransparency: number
end

declare class PVAdornment extends GuiBase3d
	Adornee: PVInstance
end

declare class HandleAdornment extends PVAdornment
	AdornCullingMode: EnumAdornCullingMode
	AlwaysOnTop: boolean
	CFrame: CFrame
	MouseButton1Down: RBXScriptSignal<>
	MouseButton1Up: RBXScriptSignal<>
	MouseEnter: RBXScriptSignal<>
	MouseLeave: RBXScriptSignal<>
	SizeRelativeOffset: Vector3
	ZIndex: number
end

declare class BoxHandleAdornment extends HandleAdornment
	Size: Vector3
end

declare class ConeHandleAdornment extends HandleAdornment
	Height: number
	Radius: number
end

declare class CylinderHandleAdornment extends HandleAdornment
	Angle: number
	Height: number
	InnerRadius: number
	Radius: number
end

declare class ImageHandleAdornment extends HandleAdornment
	Image: Content
	Size: Vector2
end

declare class LineHandleAdornment extends HandleAdornment
	Length: number
	Thickness: number
end

declare class SphereHandleAdornment extends HandleAdornment
	Radius: number
end

declare class WireframeHandleAdornment extends HandleAdornment
	Scale: Vector3
	function AddLine(self, from: Vector3, to: Vector3): nil
	function AddLines(self, points: { any }): nil
	function AddPath(self, points: { any }, loop: boolean): nil
	function Clear(self): nil
end

declare class ParabolaAdornment extends PVAdornment
	A: number
	B: number
	C: number
	Range: number
	Thickness: number
	function FindPartOnParabola(self, ignoreDescendentsTable: { Instance }): any
end

declare class SelectionSphere extends PVAdornment
	SurfaceColor3: Color3
	SurfaceTransparency: number
end

declare class PartAdornment extends GuiBase3d
	Adornee: BasePart?
end

declare class HandlesBase extends PartAdornment
end

declare class ArcHandles extends HandlesBase
	Axes: Axes
	MouseButton1Down: RBXScriptSignal<EnumAxis>
	MouseButton1Up: RBXScriptSignal<EnumAxis>
	MouseDrag: RBXScriptSignal<EnumAxis, number, number>
	MouseEnter: RBXScriptSignal<EnumAxis>
	MouseLeave: RBXScriptSignal<EnumAxis>
end

declare class Handles extends HandlesBase
	Faces: Faces
	MouseButton1Down: RBXScriptSignal<EnumNormalId>
	MouseButton1Up: RBXScriptSignal<EnumNormalId>
	MouseDrag: RBXScriptSignal<EnumNormalId, number>
	MouseEnter: RBXScriptSignal<EnumNormalId>
	MouseLeave: RBXScriptSignal<EnumNormalId>
	Style: EnumHandlesStyle
end

declare class SurfaceSelection extends PartAdornment
	TargetSurface: EnumNormalId
end

declare class SelectionLasso extends GuiBase3d
	Humanoid: Humanoid
end





declare class GuiService extends Instance
	AutoSelectGuiEnabled: boolean
	CloseInspectMenuRequest: RBXScriptSignal<>
	CoreEffectFolder: Folder
	CoreGuiFolder: Folder
	CoreGuiNavigationEnabled: boolean
	CoreGuiRenderOverflowed: RBXScriptSignal<>
	EmotesMenuOpenChanged: RBXScriptSignal<boolean>
	GuiNavigationEnabled: boolean
	GuiVisibilityChangedSignal: RBXScriptSignal<EnumGuiType, boolean>
	InspectMenuEnabledChangedSignal: RBXScriptSignal<boolean>
	InspectPlayerFromHumanoidDescriptionRequest: RBXScriptSignal<Instance, string>
	InspectPlayerFromUserIdWithCtxRequest: RBXScriptSignal<number, string>
	KeyPressed: RBXScriptSignal<string, string>
	MenuClosed: RBXScriptSignal<>
	MenuIsOpen: boolean
	MenuOpened: RBXScriptSignal<>
	NativeClose: RBXScriptSignal<>
	NetworkPausedEnabledChanged: RBXScriptSignal<boolean>
	Open9SliceEditor: RBXScriptSignal<Instance>
	PreferredTransparency: number
	PurchasePromptShown: RBXScriptSignal<>
	ReducedMotionEnabled: boolean
	SafeZoneOffsetsChanged: RBXScriptSignal<>
	SelectedCoreObject: GuiObject
	SelectedObject: GuiObject?
	SendCoreUiNotification: (title: string, text: string) -> nil
	ShowLeaveConfirmation: RBXScriptSignal<>
	SpecialKeyPressed: RBXScriptSignal<EnumSpecialKey, string>
	TouchControlsEnabled: boolean
	UiMessageChanged: RBXScriptSignal<EnumUiMessageType, string>
	function AddCenterDialog(self, dialog: Instance, centerDialogType: EnumCenterDialogType, showFunction: ((...any) -> ...any), hideFunction: ((...any) -> ...any)): nil
	function AddKey(self, key: string): nil
	function AddSelectionParent(self, selectionName: string, selectionParent: Instance): nil
	function AddSelectionTuple(self, selectionName: string, selections: any): nil
	function AddSpecialKey(self, key: EnumSpecialKey): nil
	function BroadcastNotification(self, data: string, notificationType: number): nil
	function ClearError(self): nil
	function CloseInspectMenu(self): nil
	function CloseStatsBasedOnInputString(self, input: string): boolean
	function ForceTenFootInterface(self, isForced: boolean): nil
	function GetBrickCount(self): number
	function GetClosestDialogToPosition(self, position: Vector3): Instance
	function GetEmotesMenuOpen(self): boolean
	function GetErrorCode(self): EnumConnectionError
	function GetErrorType(self): EnumConnectionError
	function GetGameplayPausedNotificationEnabled(self): boolean
	function GetGuiInset(self): (Vector2, Vector2)
	function GetGuiIsVisible(self, guiType: EnumGuiType): boolean
	function GetInspectMenuEnabled(self): boolean
	function GetNotificationTypeList(self): { [any]: any }
	function GetResolutionScale(self): number
	function GetSafeZoneOffsets(self): { [any]: any }
	function GetScreenResolution(self): Vector2
	function GetUiMessage(self): string
	function InspectPlayerFromHumanoidDescription(self, humanoidDescription: HumanoidDescription, name: string): nil
	function InspectPlayerFromUserId(self, userId: number): nil
	function InspectPlayerFromUserIdWithCtx(self, userId: number, ctx: string): nil
	function IsMemoryTrackerEnabled(self): boolean
	function IsTenFootInterface(self): boolean
	function RemoveCenterDialog(self, dialog: Instance): nil
	function RemoveKey(self, key: string): nil
	function RemoveSelectionGroup(self, selectionName: string): nil
	function RemoveSpecialKey(self, key: EnumSpecialKey): nil
	function Select(self, selectionParent: Instance): nil
	function SetEmotesMenuOpen(self, isOpen: boolean): nil
	function SetGameplayPausedNotificationEnabled(self, enabled: boolean): nil
	function SetGlobalGuiInset(self, x1: number, y1: number, x2: number, y2: number): nil
	function SetHardwareSafeAreaInsets(self, left: number, top: number, right: number, bottom: number): nil
	function SetInspectMenuEnabled(self, enabled: boolean): nil
	function SetMenuIsOpen(self, open: boolean, menuName: string?): nil
	function SetPurchasePromptIsShown(self, isShown: boolean): nil
	function SetSafeZoneOffsets(self, top: number, bottom: number, left: number, right: number): nil
	function SetUiMessage(self, msgType: EnumUiMessageType, uiMessage: string?): nil
	function ShowStatsBasedOnInputString(self, input: string): boolean
	function ToggleFullscreen(self): nil
	function ToggleGuiIsVisibleForCaptures(self, guiType: EnumGuiType): nil
	function ToggleGuiIsVisibleIfAllowed(self, guiType: EnumGuiType): nil
end

declare class GuidRegistryService extends Instance
end

declare class HapticService extends Instance
	function GetMotor(self, inputType: EnumUserInputType, vibrationMotor: EnumVibrationMotor): any
	function IsMotorSupported(self, inputType: EnumUserInputType, vibrationMotor: EnumVibrationMotor): boolean
	function IsVibrationSupported(self, inputType: EnumUserInputType): boolean
	function SetMotor(self, inputType: EnumUserInputType, vibrationMotor: EnumVibrationMotor, vibrationValues: any): nil
end

declare class HeightmapImporterService extends Instance
	ColormapHasUnknownPixels: RBXScriptSignal<>
	ProgressUpdate: RBXScriptSignal<number, string>
	function CancelImportHeightmap(self): nil
	function GetHeightmapPreviewAsync(self, heightmapAssetId: Content): any
	function ImportHeightmap(self, region: Region3, heightmapAssetId: Content, colormapAssetId: Content, defaultMaterial: EnumMaterial): nil
	function IsValidColormap(self, colormapAssetId: Content): any
	function IsValidHeightmap(self, heightmapAssetId: Content): any
	function SetImportHeightmapPaused(self, paused: boolean): nil
end

declare class HiddenSurfaceRemovalAsset extends Instance
end

declare class Highlight extends Instance
	Adornee: Instance?
	DepthMode: EnumHighlightDepthMode
	Enabled: boolean
	FillColor: Color3
	FillTransparency: number
	LineThickness: number
	OutlineColor: Color3
	OutlineTransparency: number
	ReservedId: EnumReservedHighlightId
end



declare class HttpRbxApiService extends Instance
	function GetAsync(self, apiUrlPath: string, priority: EnumThrottlingPriority?, httpRequestType: EnumHttpRequestType?): string
	function GetAsyncFullUrl(self, apiUrl: string, priority: EnumThrottlingPriority?, httpRequestType: EnumHttpRequestType?): string
	function GetDocumentationUrl(self, partialUrl: string): string
	function PostAsync(self, apiUrlPath: string, data: string, priority: EnumThrottlingPriority?, content_type: EnumHttpContentType?, httpRequestType: EnumHttpRequestType?): string
	function PostAsyncFullUrl(self, apiUrl: string, data: string, priority: EnumThrottlingPriority?, content_type: EnumHttpContentType?, httpRequestType: EnumHttpRequestType?): string
	function RequestAsync(self, requestOptions: { [any]: any }, priority: EnumThrottlingPriority?, content_type: EnumHttpContentType?, httpRequestType: EnumHttpRequestType?): string
	function RequestLimitedAsync(self, requestOptions: { [any]: any }, priority: EnumThrottlingPriority?, content_type: EnumHttpContentType?, httpRequestType: EnumHttpRequestType?): string
end

declare class HttpRequest extends Instance
	function Cancel(self): nil
	function Start(self, callback: ((...any) -> ...any)): nil
end

declare class HttpService extends Instance
	HttpEnabled: boolean
	function GenerateGUID(self, wrapInCurlyBraces: boolean?): string
	function GetAsync(self, url: string, nocache: boolean?, headers: any): string
	function GetHttpEnabled(self): boolean
	function GetUserAgent(self): string
	function JSONDecode(self, input: string): any
	function JSONEncode(self, input: any): string
	function PostAsync(self, url: string, data: string, content_type: EnumHttpContentType?, compress: boolean?, headers: any): string
	function RequestAsync(self, options: HttpRequestOptions): HttpResponseData
	function RequestInternal(self, options: { [any]: any }): Instance
	function SetHttpEnabled(self, enabled: boolean): nil
	function UrlEncode(self, input: string): string
end

declare class Humanoid extends Instance
	AutoJumpEnabled: boolean
	AutoRotate: boolean
	AutomaticScalingEnabled: boolean
	BreakJointsOnDeath: boolean
	CameraOffset: Vector3
	Climbing: RBXScriptSignal<number>
	ClusterCompositionFinished: RBXScriptSignal<>
	Died: RBXScriptSignal<>
	DisplayDistanceType: EnumHumanoidDisplayDistanceType
	DisplayName: string
	EmoteTriggered: RBXScriptSignal<boolean, AnimationTrack>
	EvaluateStateMachine: boolean
	FallingDown: RBXScriptSignal<boolean>
	FloorMaterial: EnumMaterial
	FreeFalling: RBXScriptSignal<boolean>
	GettingUp: RBXScriptSignal<boolean>
	Health: number
	HealthChanged: RBXScriptSignal<number>
	HealthDisplayDistance: number
	HealthDisplayType: EnumHumanoidHealthDisplayType
	HipHeight: number
	InternalDisplayName: string
	Jump: boolean
	JumpHeight: number
	JumpPower: number
	Jumping: RBXScriptSignal<boolean>
	MaxHealth: number
	MaxSlopeAngle: number
	MoveDirection: Vector3
	MoveToFinished: RBXScriptSignal<boolean>
	NameDisplayDistance: number
	NameOcclusion: EnumNameOcclusion
	PlatformStand: boolean
	PlatformStanding: RBXScriptSignal<boolean>
	Ragdoll: RBXScriptSignal<boolean>
	RequiresNeck: boolean
	RigType: EnumHumanoidRigType
	RootPart: BasePart?
	Running: RBXScriptSignal<number>
	SeatPart: Seat | VehicleSeat | nil
	Seated: RBXScriptSignal<boolean, Seat>
	Sit: boolean
	StateChanged: RBXScriptSignal<EnumHumanoidStateType, EnumHumanoidStateType>
	StateEnabledChanged: RBXScriptSignal<EnumHumanoidStateType, boolean>
	Strafing: RBXScriptSignal<boolean>
	Swimming: RBXScriptSignal<number>
	TargetPoint: Vector3
	Touched: RBXScriptSignal<BasePart, BasePart>
	UseJumpPower: boolean
	WalkSpeed: number
	WalkToPart: BasePart?
	WalkToPoint: Vector3
	function AddAccessory(self, accessory: Accessory): nil
	function ApplyDescription(self, humanoidDescription: HumanoidDescription, assetTypeVerification: EnumAssetTypeVerification?): nil
	function ApplyDescriptionBlocking(self, humanoidDescription: HumanoidDescription): nil
	function ApplyDescriptionClientServer(self, humanoidDescription: HumanoidDescription): nil
	function ApplyDescriptionReset(self, humanoidDescription: HumanoidDescription, assetTypeVerification: EnumAssetTypeVerification?): nil
	function BuildRigFromAttachments(self): nil
	function CacheDefaults(self): nil
	function ChangeState(self, state: EnumHumanoidStateType?): nil
	function EquipTool(self, tool: Tool): nil
	function GetAccessories(self): { Accessory }
	function GetAccessoryHandleScale(self, instance: Instance, partType: EnumBodyPartR15): Vector3
	function GetAppliedDescription(self): HumanoidDescription
	function GetBodyPartR15(self, part: BasePart): EnumBodyPartR15
	function GetLimb(self, part: BasePart): EnumLimb
	function GetMoveVelocity(self): Vector3
	function GetState(self): EnumHumanoidStateType
	function GetStateEnabled(self, state: EnumHumanoidStateType): boolean
	function Move(self, moveDirection: Vector3, relativeToCamera: boolean?): nil
	function MoveTo(self, location: Vector3, part: BasePart?): nil
	function PlayEmote(self, emoteName: string): boolean
	function PlayEmoteAndGetAnimTrackById(self, emoteId: number): any
	function RemoveAccessories(self): nil
	function ReplaceBodyPartR15(self, bodyPart: EnumBodyPartR15, part: BasePart): boolean
	function SetClickToWalkEnabled(self, enabled: boolean): nil
	function SetStateEnabled(self, state: EnumHumanoidStateType, enabled: boolean): nil
	function TakeDamage(self, amount: number): nil
	function UnequipTools(self): nil
end

declare class HumanoidDescription extends Instance
	AccessoryBlob: string
	BackAccessory: string
	BodyTypeScale: number
	ClimbAnimation: number
	DepthScale: number
	EmotesChanged: RBXScriptSignal<{ [any]: any }>
	EquippedEmotesChanged: RBXScriptSignal<{ any }>
	Face: number
	FaceAccessory: string
	FallAnimation: number
	FrontAccessory: string
	GraphicTShirt: number
	HairAccessory: string
	HatAccessory: string
	Head: number
	HeadColor: Color3
	HeadScale: number
	HeightScale: number
	IdleAnimation: number
	JumpAnimation: number
	LeftArm: number
	LeftArmColor: Color3
	LeftLeg: number
	LeftLegColor: Color3
	MoodAnimation: number
	NeckAccessory: string
	NumberEmotesLoaded: number
	Pants: number
	ProportionScale: number
	RightArm: number
	RightArmColor: Color3
	RightLeg: number
	RightLegColor: Color3
	RunAnimation: number
	Shirt: number
	ShouldersAccessory: string
	SwimAnimation: number
	Torso: number
	TorsoColor: Color3
	WaistAccessory: string
	WalkAnimation: number
	WidthScale: number
	function AddEmote(self, name: string, assetId: number): nil
	function GetAccessories(self, includeRigidAccessories: boolean): { HumanoidDescriptionAccessory }
	function GetEmotes(self): { [string]: { number } }
	function GetEquippedEmotes(self): { { Slot: number, Name: string } }
	function RemoveEmote(self, name: string): nil
	function SetAccessories(self, accessories: { HumanoidDescriptionAccessory }, includeRigidAccessories: boolean): ()
	function SetEmotes(self, emotes: { [string]: { number } }): ()
	function SetEquippedEmotes(self, equippedEmotes: { string } | { Slot: number, Name: string }): ()
end

declare class IKControl extends Instance
	ChainRoot: Instance
	Enabled: boolean
	EndEffector: Instance
	EndEffectorOffset: CFrame
	Offset: CFrame
	Pole: Instance
	Priority: number
	SmoothTime: number
	Target: Instance
	Type: EnumIKControlType
	Weight: number
	function GetChainCount(self): number
	function GetChainLength(self): number
	function GetNodeLocalCFrame(self, index: number): CFrame
	function GetNodeWorldCFrame(self, index: number): CFrame
	function GetRawFinalTarget(self): CFrame
	function GetSmoothedFinalTarget(self): CFrame
end

declare class ILegacyStudioBridge extends Instance
end

declare class LegacyStudioBridge extends ILegacyStudioBridge
end

declare class IXPService extends Instance
	OnBrowserTrackerLayerLoadingStatusChanged: RBXScriptSignal<EnumIXPLoadingStatus>
	OnUserLayerLoadingStatusChanged: RBXScriptSignal<EnumIXPLoadingStatus>
	function ClearUserLayers(self): nil
	function GetBrowserTrackerLayerLoadingStatus(self): EnumIXPLoadingStatus
	function GetBrowserTrackerLayerVariables(self, layerName: string): { [any]: any }
	function GetBrowserTrackerStatusForLayer(self, layerName: string): EnumIXPLoadingStatus?
	function GetRegisteredUserLayersToStatus(self): { [any]: any }
	function GetUserLayerLoadingStatus(self): EnumIXPLoadingStatus
	function GetUserLayerVariables(self, layerName: string): { [any]: any }
	function GetUserStatusForLayer(self, layerName: string): EnumIXPLoadingStatus?
	function InitializeUserLayers(self, userId: number): nil
	function LogBrowserTrackerLayerExposure(self, layerName: string): nil
	function LogUserLayerExposure(self, layerName: string): nil
	function RegisterUserLayers(self, userLayers: any): nil
end

declare class IncrementalPatchBuilder extends Instance
	AddPathsToBundle: boolean
	BuildDebouncePeriod: number
	HighCompression: boolean
	SerializePatch: boolean
	ZstdCompression: boolean
end

declare class InputObject extends Instance
	Delta: Vector3
	KeyCode: EnumKeyCode
	Position: Vector3
	UserInputState: EnumUserInputState
	UserInputType: EnumUserInputType
	function IsModifierKeyDown(self, modifierKey: EnumModifierKey): boolean
end

declare class InsertService extends Instance
	AllowClientInsertModels: boolean
	function CreateMeshPartAsync(self, meshId: Content, collisionFidelity: EnumCollisionFidelity, renderFidelity: EnumRenderFidelity): MeshPart
	function GetBaseSets(self): { any }
	function GetCollection(self, categoryId: number): { any }
	function GetFreeDecals(self, searchText: string, pageNum: number): { any }
	function GetFreeModels(self, searchText: string, pageNum: number): { any }
	function GetLatestAssetVersionAsync(self, assetId: number): number
	function GetLocalFileContents(self, contentId: string): string
	function GetUserSets(self, userId: number): { any }
	function LoadAsset(self, assetId: number): Instance
	function LoadAssetVersion(self, assetVersionId: number): Instance
	function LoadAssetWithFormat(self, assetId: number, format: string): { Instance }
	function LoadLocalAsset(self, assetPath: string): Instance
	function LoadPackageAsset(self, url: Content): { Instance }
	function LoadPackageAssetAsync(self, url: Content): { Instance }
end

declare class JointInstance extends Instance
	Active: boolean
	C0: CFrame
	C1: CFrame
	Enabled: boolean
	Part0: BasePart?
	Part1: BasePart?
end

declare class DynamicRotate extends JointInstance
	BaseAngle: number
end













declare class Motor extends JointInstance
	CurrentAngle: number
	DesiredAngle: number
	MaxVelocity: number
	function SetDesiredAngle(self, value: number): nil
end

declare class Motor6D extends Motor
	ChildName: string
	ParentName: string
	Transform: CFrame
end





declare class VelocityMotor extends JointInstance
	CurrentAngle: number
	DesiredAngle: number
	Hole: Hole
	MaxVelocity: number
end

declare class Weld extends JointInstance
end



declare class KeyboardService extends Instance
end

declare class Keyframe extends Instance
	Time: number
	function AddMarker(self, marker: KeyframeMarker): nil
	function AddPose(self, pose: Pose): nil
	function GetMarkers(self): { Instance }
	function GetPoses(self): { Instance }
	function RemoveMarker(self, marker: Instance): nil
	function RemovePose(self, pose: Pose): nil
end

declare class KeyframeMarker extends Instance
	Value: string
end

declare class KeyframeSequenceProvider extends Instance
	function GetAnimations(self, userId: number): Instance
	function GetKeyframeSequenceAsync(self, assetId: Content): Instance
	function GetMemStats(self): { [any]: any }
	function RegisterActiveKeyframeSequence(self, keyframeSequence: Instance): Content
	function RegisterKeyframeSequence(self, keyframeSequence: Instance): Content
end

declare class LSPFileSyncService extends Instance
end

declare class LanguageService extends Instance
end

declare class Light extends Instance
	Brightness: number
	Color: Color3
	Enabled: boolean
	Shadows: boolean
end

declare class PointLight extends Light
	Range: number
end

declare class SpotLight extends Light
	Angle: number
	Face: EnumNormalId
	Range: number
end

declare class SurfaceLight extends Light
	Angle: number
	Face: EnumNormalId
	Range: number
end

declare class Lighting extends Instance
	Ambient: Color3
	Brightness: number
	ClockTime: number
	ColorShift_Bottom: Color3
	ColorShift_Top: Color3
	EnvironmentDiffuseScale: number
	EnvironmentSpecularScale: number
	ExposureCompensation: number
	FogColor: Color3
	FogEnd: number
	FogStart: number
	GeographicLatitude: number
	GlobalShadows: boolean
	LightingChanged: RBXScriptSignal<boolean>
	OutdoorAmbient: Color3
	ShadowSoftness: number
	Technology: EnumTechnology
	TempUseNewSkyRemovalBehaviour: boolean
	TimeOfDay: string
	function GetMinutesAfterMidnight(self): number
	function GetMoonDirection(self): Vector3
	function GetMoonPhase(self): number
	function GetSunDirection(self): Vector3
	function SetMinutesAfterMidnight(self, minutes: number): nil
end

declare class LiveScriptingService extends Instance
end

declare class LocalStorageService extends Instance
	ItemWasSet: RBXScriptSignal<string, string>
	StoreWasCleared: RBXScriptSignal<>
	function Flush(self): nil
	function GetItem(self, key: string): string
	function SetItem(self, key: string, value: string): nil
	function WhenLoaded(self, callback: ((...any) -> ...any)): nil
end

declare class AppStorageService extends LocalStorageService
end

declare class UserStorageService extends LocalStorageService
end

declare class LocalizationService extends Instance
	AutoTranslateWillRun: RBXScriptSignal<>
	ForcePlayModeGameLocaleId: string
	ForcePlayModeRobloxLocaleId: string
	IsTextScraperRunning: boolean
	RobloxForcePlayModeGameLocaleId: string
	RobloxForcePlayModeRobloxLocaleId: string
	RobloxLocaleId: string
	SystemLocaleId: string
	function GetCorescriptLocalizations(self): { Instance }
	function GetCountryRegionForPlayerAsync(self, player: Player): string
	function GetTableEntries(self, instance: Instance?): { any }
	function GetTranslatorForLocaleAsync(self, locale: string): Translator
	function GetTranslatorForPlayer(self, player: Player): Translator
	function GetTranslatorForPlayerAsync(self, player: Player): Translator
	function PromptDownloadGameTableToCSV(self, table: Instance): nil
	function PromptExportToCSVs(self): nil
	function PromptImportFromCSVs(self): nil
	function PromptUploadCSVToGameTable(self): Instance
	function SetRobloxLocaleId(self, locale: string): nil
	function StartTextScraper(self): nil
	function StopTextScraper(self): nil
end

declare class LocalizationTable extends Instance
	SourceLocaleId: string
	function GetEntries(self): { any }
	function GetTranslator(self, localeId: string): Translator
	function RemoveEntry(self, key: string, source: string, context: string): nil
	function RemoveEntryValue(self, key: string, source: string, context: string, localeId: string): nil
	function RemoveTargetLocale(self, localeId: string): nil
	function SetEntries(self, entries: any): nil
	function SetEntryContext(self, key: string, source: string, context: string, newContext: string): nil
	function SetEntryExample(self, key: string, source: string, context: string, example: string): nil
	function SetEntryKey(self, key: string, source: string, context: string, newKey: string): nil
	function SetEntrySource(self, key: string, source: string, context: string, newSource: string): nil
	function SetEntryValue(self, key: string, source: string, context: string, localeId: string, text: string): nil
	function SetIsExemptFromUGCAnalytics(self, value: boolean): nil
end

declare class CloudLocalizationTable extends LocalizationTable
end

declare class LodDataEntity extends Instance
	EntityLodEnabled: boolean
end

declare class LodDataService extends Instance
end

declare class LogService extends Instance
	HttpResultOut: RBXScriptSignal<{ [any]: any }>
	MessageOut: RBXScriptSignal<string, EnumMessageType>
	OnHttpResultApproved: RBXScriptSignal<boolean>
	ServerHttpResultOut: RBXScriptSignal<{ [any]: any }>
	ServerMessageOut: RBXScriptSignal<string, EnumMessageType, number>
	function ClearOutput(self): nil
	function ExecuteScript(self, source: string): nil
	function GetHttpResultHistory(self): { any }
	function GetLogHistory(self): { any }
	function RequestHttpResultApproved(self): nil
	function RequestServerHttpResult(self): nil
	function RequestServerOutput(self): nil
end

declare class LoginService extends Instance
	LoginFailed: RBXScriptSignal<string>
	LoginSucceeded: RBXScriptSignal<string>
	function Logout(self): nil
	function PromptLogin(self): nil
end

declare class LuaSettings extends Instance
end

declare class LuaSourceContainer extends Instance
	CurrentEditor: Instance
	RuntimeSource: string
end

declare class BaseScript extends LuaSourceContainer
	Disabled: boolean
	Enabled: boolean
	LinkedSource: Content
	RunContext: EnumRunContext
end

declare class CoreScript extends BaseScript
end

declare class Script extends BaseScript
	Source: ProtectedString
	function GetHash(self): string
end

declare class LocalScript extends Script
end

declare class ModuleScript extends LuaSourceContainer
	LinkedSource: Content
	Source: ProtectedString
end

declare class LuaWebService extends Instance
end

declare class LuauScriptAnalyzerService extends Instance
end

declare class MarkerCurve extends Instance
	Length: number
	function GetMarkerAtIndex(self, index: number): { [any]: any }
	function GetMarkers(self): { any }
	function InsertMarkerAtTime(self, time: number, marker: string): { any }
	function RemoveMarkerAtIndex(self, startingIndex: number, count: number?): number
end

declare class MarketplaceService extends Instance
	ClientLuaDialogRequested: RBXScriptSignal<any>
	ClientPurchaseSuccess: RBXScriptSignal<string, number, number>
	NativePurchaseFinished: RBXScriptSignal<Player, string, boolean>
	NativePurchaseFinishedWithLocalPlayer: RBXScriptSignal<string, boolean>
	PrepareCollectiblesPurchaseRequested: RBXScriptSignal<Instance, string, string, string, number>
	ProcessReceipt: (receiptInfo: { [any]: any }) -> EnumProductPurchaseDecision
	PromptBundlePurchaseFinished: RBXScriptSignal<Instance, number, boolean>
	PromptBundlePurchaseRequested: RBXScriptSignal<Instance, number>
	PromptCollectiblesPurchaseRequested: RBXScriptSignal<Instance, string, string, string, number, string, string>
	PromptGamePassPurchaseFinished: RBXScriptSignal<Player, number, boolean>
	PromptGamePassPurchaseRequested: RBXScriptSignal<Player, number>
	PromptPremiumPurchaseFinished: RBXScriptSignal<>
	PromptPremiumPurchaseRequested: RBXScriptSignal<Instance>
	PromptProductPurchaseFinished: RBXScriptSignal<number, number, boolean>
	PromptProductPurchaseRequested: RBXScriptSignal<Player, number, boolean, EnumCurrencyType>
	PromptPurchaseFinished: RBXScriptSignal<Player, number, boolean>
	PromptPurchaseRequested: RBXScriptSignal<Player, number, boolean, EnumCurrencyType>
	PromptPurchaseRequestedV2: RBXScriptSignal<Instance, number, boolean, EnumCurrencyType, string, string>
	PromptRobloxPurchaseRequested: RBXScriptSignal<number, boolean>
	PromptSubscriptionCancellationRequested: RBXScriptSignal<Instance, number>
	PromptSubscriptionPurchaseRequested: RBXScriptSignal<Player, number>
	ServerPurchaseVerification: RBXScriptSignal<{ [any]: any }>
	ThirdPartyPurchaseFinished: RBXScriptSignal<Instance, string, string, boolean>
	function GetDeveloperProductsAsync(self): Pages
	function GetProductInfo(self, assetId: number, infoType: EnumInfoType?): { [any]: any }
	function GetRobuxBalance(self): number
	function PerformPurchase(self, infoType: EnumInfoType, productId: number, expectedPrice: number, requestId: string, isRobloxPurchase: boolean, collectibleItemId: string?, collectibleProductId: string?, idempotencyKey: string?, purchaseAuthToken: string?): { [any]: any }
	function PerformPurchaseV2(self, infoType: EnumInfoType, productId: number, expectedPrice: number, requestId: string, isRobloxPurchase: boolean, collectiblesProductDetails: { [any]: any }): { [any]: any }
	function PlayerCanMakePurchases(self, player: Instance): boolean
	function PlayerOwnsAsset(self, player: Player, assetId: number): boolean
	function PlayerOwnsBundle(self, player: Player, bundleId: number): boolean
	function PrepareCollectiblesPurchase(self, player: Instance, collectibleItemId: string, collectibleItemInstanceId: string, collectibleProductId: string, expectedPrice: number): nil
	function PromptBundlePurchase(self, player: Player, bundleId: number): nil
	function PromptCollectiblesPurchase(self, player: Instance, collectibleItemId: string, collectibleItemInstanceId: string, collectibleProductId: string, expectedPrice: number): nil
	function PromptGamePassPurchase(self, player: Player, gamePassId: number): nil
	function PromptNativePurchase(self, player: Instance, productId: string): nil
	function PromptNativePurchaseWithLocalPlayer(self, productId: string): nil
	function PromptPremiumPurchase(self, player: Player): nil
	function PromptProductPurchase(self, player: Player, productId: number, equipIfPurchased: boolean?, currencyType: EnumCurrencyType?): nil
	function PromptPurchase(self, player: Player, assetId: number, equipIfPurchased: boolean?, currencyType: EnumCurrencyType?): nil
	function PromptRobloxPurchase(self, assetId: number, equipIfPurchased: boolean): nil
	function PromptThirdPartyPurchase(self, player: Instance, productId: string): nil
	function ReportAssetSale(self, assetId: string, robuxAmount: number): nil
	function ReportRobuxUpsellStarted(self): nil
	function SignalAssetTypePurchased(self, player: Instance, assetType: EnumAssetType): nil
	function SignalClientPurchaseSuccess(self, ticket: string, playerId: number, productId: number): nil
	function SignalMockPurchasePremium(self): nil
	function SignalPromptBundlePurchaseFinished(self, player: Instance, bundleId: number, success: boolean): nil
	function SignalPromptGamePassPurchaseFinished(self, player: Instance, gamePassId: number, success: boolean): nil
	function SignalPromptPremiumPurchaseFinished(self, didTryPurchasing: boolean): nil
	function SignalPromptProductPurchaseFinished(self, userId: number, productId: number, success: boolean): nil
	function SignalPromptPurchaseFinished(self, player: Instance, assetId: number, success: boolean): nil
	function SignalPromptSubscriptionCancellationFinished(self, player: Instance, subscriptionId: number, wasCanceled: boolean): nil
	function SignalPromptSubscriptionPurchaseFinished(self, player: Instance, subscriptionId: number, wasPurchased: boolean): nil
	function SignalServerLuaDialogClosed(self, value: boolean): nil
	function UserOwnsGamePassAsync(self, userId: number, gamePassId: number): boolean
end

declare class MaterialGenerationService extends Instance
	function GetAccountingBalanceAsync(self): number
	function RefillAccountingBalanceAsync(self): number
	function StartSession(self): MaterialGenerationSession
end

declare class MaterialGenerationSession extends Instance
	function GenerateImagesAsync(self, prompt: string, options: { [any]: any }): any
	function GenerateMaterialMapsAsync(self, imageId: string): { [any]: any }
	function UploadMaterialAsync(self, imageId: string): { [any]: any }
end

declare class MaterialService extends Instance
	AsphaltName: string
	BasaltName: string
	BrickName: string
	CobblestoneName: string
	ConcreteName: string
	CorrodedMetalName: string
	CrackedLavaName: string
	DiamondPlateName: string
	FabricName: string
	FoilName: string
	GlacierName: string
	GraniteName: string
	GrassName: string
	GroundName: string
	IceName: string
	LeafyGrassName: string
	LimestoneName: string
	MarbleName: string
	MetalName: string
	MudName: string
	OverrideStatusChanged: RBXScriptSignal<EnumMaterial>
	PavementName: string
	PebbleName: string
	PlasticName: string
	RockName: string
	SaltName: string
	SandName: string
	SandstoneName: string
	SlateName: string
	SmoothPlasticName: string
	SnowName: string
	Use2022Materials: boolean
	WoodName: string
	WoodPlanksName: string
	function GetBaseMaterialOverride(self, material: EnumMaterial): string
	function GetMaterialOverrideChanged(self, material: EnumMaterial): RBXScriptSignal
	function GetMaterialVariant(self, material: EnumMaterial, name: string): MaterialVariant
	function GetOverrideStatus(self, material: EnumMaterial): EnumPropertyStatus
	function SetBaseMaterialOverride(self, material: EnumMaterial, name: string): nil
end

declare class MaterialVariant extends Instance
	BaseMaterial: EnumMaterial
	ColorMap: Content
	CustomPhysicalProperties: PhysicalProperties
	MaterialPattern: EnumMaterialPattern
	MetalnessMap: Content
	NormalMap: Content
	RoughnessMap: Content
	StudsPerTile: number
end

declare class MemStorageConnection extends Instance
	function Disconnect(self): nil
end

declare class MemStorageService extends Instance
	function Bind(self, key: string, callback: ((...any) -> ...any)): MemStorageConnection
	function BindAndFire(self, key: string, callback: ((...any) -> ...any)): MemStorageConnection
	function Call(self, key: string, input: any): any
	function Fire(self, key: string, value: string?): nil
	function GetItem(self, key: string, defaultValue: string?): string
	function HasItem(self, key: string): boolean
	function RemoveItem(self, key: string): boolean
	function SetItem(self, key: string, value: string?): nil
end

declare class MemoryStoreQueue extends Instance
	function AddAsync(self, value: any, expiration: number, priority: number?): nil
	function ReadAsync(self, count: number, allOrNothing: boolean?, waitTimeout: number?): any
	function RemoveAsync(self, id: string): nil
end

declare class MemoryStoreService extends Instance
	function GetQueue(self, name: string, invisibilityTimeout: number?): MemoryStoreQueue
	function GetSortedMap(self, name: string): MemoryStoreSortedMap
end

declare class MemoryStoreSortedMap extends Instance
	function GetAsync(self, key: string): any
	function GetRangeAsync(self, direction: EnumSortDirection, count: number, exclusiveLowerBound: string?, exclusiveUpperBound: string?): { any }
	function RemoveAsync(self, key: string): nil
	function SetAsync(self, key: string, value: any, expiration: number): boolean
	function UpdateAsync(self, key: string, transformFunction: ((...any) -> ...any), expiration: number): any
end





declare class MessageBusConnection extends Instance
	function Disconnect(self): nil
end

declare class MessageBusService extends Instance
	function Call(self, key: string, input: any): any
	function GetLast(self, mid: string): any
	function GetMessageId(self, domainName: string, messageName: string): string
	function GetProtocolMethodRequestMessageId(self, protocolName: string, methodName: string): string
	function GetProtocolMethodResponseMessageId(self, protocolName: string, methodName: string): string
	function MakeRequest(self, protocolName: string, methodName: string, message: any, callback: ((...any) -> ...any), customTelemetryData: any): nil
	function Publish(self, mid: string, params: any): nil
	function PublishProtocolMethodRequest(self, protocolName: string, methodName: string, message: any, customTelemetryData: any): nil
	function PublishProtocolMethodResponse(self, protocolName: string, methodName: string, message: any, responseCode: number, customTelemetryData: any): nil
	function SetRequestHandler(self, protocolName: string, methodName: string, callback: ((...any) -> ...any)): nil
	function Subscribe(self, mid: string, callback: ((...any) -> ...any), once: boolean, sticky: boolean): Instance
	function SubscribeToProtocolMethodRequest(self, protocolName: string, methodName: string, callback: ((...any) -> ...any), once: boolean, sticky: boolean): Instance
	function SubscribeToProtocolMethodResponse(self, protocolName: string, methodName: string, callback: ((...any) -> ...any), once: boolean, sticky: boolean): Instance
end

declare class MessagingService extends Instance
	function PublishAsync(self, topic: string, message: any): nil
	function SubscribeAsync(self, topic: string, callback: ((...any) -> ...any)): RBXScriptConnection
end

declare class MetaBreakpoint extends Instance
	Condition: string
	ContinueExecution: boolean
	Enabled: boolean
	Id: number
	IsLogpoint: boolean
	Line: number
	LogMessage: string
	RemoveOnHit: boolean
	Script: string
	Valid: boolean
	function GetContextBreakpoints(self): { [any]: any }
	function Remove(self, status: ((...any) -> ...any)): number
	function SetChildBreakpointEnabledByScriptAndContext(self, script: string, contextGST: number, enabled: boolean): nil
	function SetContextEnabled(self, context: number, enabled: boolean): nil
	function SetContinueExecution(self, enabled: boolean): nil
	function SetEnabled(self, enabled: boolean): nil
	function SetLine(self, line: number, status: ((...any) -> ...any)): number
	function SetRemoveOnHit(self, enabled: boolean): nil
end

declare class MetaBreakpointContext extends Instance
end

declare class MetaBreakpointManager extends Instance
	MetaBreakpointAdded: RBXScriptSignal<MetaBreakpoint>
	MetaBreakpointChanged: RBXScriptSignal<MetaBreakpoint>
	MetaBreakpointRemoved: RBXScriptSignal<MetaBreakpoint>
	MetaBreakpointSetChanged: RBXScriptSignal<MetaBreakpoint, { [any]: any }>
	function AddBreakpoint(self, script: Instance, line: number, condition: Instance): Instance
	function GetBreakpointById(self, metaBreakpointId: number): MetaBreakpoint
	function RemoveBreakpointById(self, metaBreakpointId: number): nil
end

declare class Mouse extends Instance
	Button1Down: RBXScriptSignal<>
	Button1Up: RBXScriptSignal<>
	Button2Down: RBXScriptSignal<>
	Button2Up: RBXScriptSignal<>
	Hit: CFrame
	Icon: Content
	Idle: RBXScriptSignal<>
	Move: RBXScriptSignal<>
	Origin: CFrame
	Target: BasePart
	TargetFilter: Instance
	TargetSurface: EnumNormalId
	UnitRay: Ray
	ViewSizeX: number
	ViewSizeY: number
	WheelBackward: RBXScriptSignal<>
	WheelForward: RBXScriptSignal<>
	X: number
	Y: number
end

declare class PlayerMouse extends Mouse
end

declare class PluginMouse extends Mouse
	DragEnter: RBXScriptSignal<{ Instance }>
end

declare class MouseService extends Instance
	MouseEnterStudioViewport: RBXScriptSignal<>
	MouseLeaveStudioViewport: RBXScriptSignal<>
end

declare class MultipleDocumentInterfaceInstance extends Instance
	DataModelSessionEnded: RBXScriptSignal<Instance>
	DataModelSessionStarted: RBXScriptSignal<Instance>
	FocusedDataModelSession: Instance
end

declare class NetworkMarker extends Instance
	Received: RBXScriptSignal<>
end

declare class NetworkPeer extends Instance
	function SetOutgoingKBPSLimit(self, limit: number): nil
end

declare class NetworkClient extends NetworkPeer
	ConnectionAccepted: RBXScriptSignal<string, Instance>
	ConnectionFailed: RBXScriptSignal<string, number, string>
end

declare class NetworkServer extends NetworkPeer
	function EncryptStringForPlayerId(self, toEncrypt: string, playerId: number): string
end

declare class NetworkReplicator extends Instance
	function GetPlayer(self): Instance
end

declare class ClientReplicator extends NetworkReplicator
	RCCProfilerDataComplete: RBXScriptSignal<boolean, string>
	StatsReceived: RBXScriptSignal<{ [any]: any }>
	function RequestRCCProfilerData(self, frameRate: number, timeFrame: number): nil
	function RequestServerScriptProfiling(self, start: boolean, frequency: number?): nil
	function RequestServerStats(self, request: boolean): nil
end

declare class ServerReplicator extends NetworkReplicator
end

declare class NetworkSettings extends Instance
	EmulatedTotalMemoryInMB: number
	FreeMemoryMBytes: number
	HttpProxyEnabled: boolean
	HttpProxyURL: string
	IncomingReplicationLag: number
	PrintJoinSizeBreakdown: boolean
	PrintPhysicsErrors: boolean
	PrintStreamInstanceQuota: boolean
	RandomizeJoinInstanceOrder: boolean
	RenderStreamedRegions: boolean
	ShowActiveAnimationAsset: boolean
end

declare class NoCollisionConstraint extends Instance
	Enabled: boolean
	Part0: BasePart
	Part1: BasePart
end

declare class NotificationService extends Instance
	IsConnected: boolean
	IsLuaChatEnabled: boolean
	IsLuaGameDetailsEnabled: boolean
	Roblox17sConnectionChanged: RBXScriptSignal<string, EnumConnectionState, string>
	Roblox17sEventReceived: RBXScriptSignal<{ [any]: any }>
	RobloxConnectionChanged: RBXScriptSignal<string, EnumConnectionState, string, string>
	RobloxEventReceived: RBXScriptSignal<{ [any]: any }>
	SelectedTheme: string
	function ActionEnabled(self, actionType: EnumAppShellActionType): nil
	function ActionTaken(self, actionType: EnumAppShellActionType): nil
	function CancelAllNotification(self, userId: number): nil
	function CancelNotification(self, userId: number, alertId: number): nil
	function GetScheduledNotifications(self, userId: number): { any }
	function ScheduleNotification(self, userId: number, alertId: number, alertMsg: string, minutesToFire: number): nil
	function SwitchedToAppShellFeature(self, appShellFeature: EnumAppShellFeature): nil
end

declare class OmniRecommendationsService extends Instance
	function ClearSessionId(self): nil
	function GetSessionId(self): string
	function MakeRequest(self, nextPageToken: string): HttpRequest
end

declare class OpenCloudApiV1 extends Instance
	function CreateModel(self, name: string): OpenCloudModel
	function CreateUserNotificationAsync(self, user: string, userNotification: OpenCloudModel): OpenCloudModel
end

declare class OpenCloudService extends Instance
	function GetApiV1(self): OpenCloudApiV1
end

declare class PVInstance extends Instance
	Origin: CFrame
	function GetPivot(self): CFrame
	function PivotTo(self, targetCFrame: CFrame): nil
end

declare class BasePart extends PVInstance
	Anchored: boolean
	AssemblyAngularVelocity: Vector3
	AssemblyCenterOfMass: Vector3
	AssemblyLinearVelocity: Vector3
	AssemblyMass: number
	AssemblyRootPart: BasePart
	BackSurface: EnumSurfaceType
	BottomSurface: EnumSurfaceType
	BrickColor: BrickColor
	CFrame: CFrame
	CanCollide: boolean
	CanQuery: boolean
	CanTouch: boolean
	CastShadow: boolean
	CenterOfMass: Vector3
	CollisionGroup: string
	Color: Color3
	CurrentPhysicalProperties: PhysicalProperties
	CustomPhysicalProperties: PhysicalProperties
	EnableFluidForces: boolean
	ExtentsCFrame: CFrame
	ExtentsSize: Vector3
	FrontSurface: EnumSurfaceType
	LeftSurface: EnumSurfaceType
	LocalTransparencyModifier: number
	Locked: boolean
	Mass: number
	Massless: boolean
	Material: EnumMaterial
	MaterialVariant: string
	Orientation: Vector3
	PivotOffset: CFrame
	Position: Vector3
	ReceiveAge: number
	Reflectance: number
	ResizeIncrement: number
	ResizeableFaces: Faces
	RightSurface: EnumSurfaceType
	RootPriority: number
	Rotation: Vector3
	Size: Vector3
	TopSurface: EnumSurfaceType
	TouchEnded: RBXScriptSignal<BasePart>
	Touched: RBXScriptSignal<BasePart>
	Transparency: number
	function ApplyAngularImpulse(self, impulse: Vector3): nil
	function ApplyImpulse(self, impulse: Vector3): nil
	function ApplyImpulseAtPosition(self, impulse: Vector3, position: Vector3): nil
	function CanCollideWith(self, part: BasePart): boolean
	function CanSetNetworkOwnership(self): (boolean, string)
	function GetClosestPointOnSurface(self, position: Vector3): Vector3
	function GetConnectedParts(self, recursive: boolean?): { BasePart }
	function GetJoints(self): { BasePart }
	function GetMass(self): number
	function GetNetworkOwner(self): Player?
	function GetNetworkOwnershipAuto(self): boolean
	function GetNoCollisionConstraints(self): { Instance }
	function GetRootPart(self): BasePart
	function GetTouchingParts(self): { BasePart }
	function GetVelocityAtPosition(self, position: Vector3): Vector3
	function IntersectAsync(self, parts: { Instance }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): Instance
	function IsGrounded(self): boolean
	function Resize(self, normalId: EnumNormalId, deltaAmount: number): boolean
	function SetNetworkOwner(self, playerInstance: Player?): nil
	function SetNetworkOwnershipAuto(self): nil
	function SubtractAsync(self, parts: { BasePart }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): UnionOperation
	function UnionAsync(self, parts: { BasePart }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): UnionOperation
	function subtractAsync2_INTERNAL(self, parts: { Instance }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): { Instance }
	function unionAsync2_INTERNAL(self, parts: { Instance }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): { Instance }
end

declare class CornerWedgePart extends BasePart
end

declare class FormFactorPart extends BasePart
end

declare class Part extends FormFactorPart
	Shape: EnumPartType
end



declare class Platform extends Part
end

declare class Seat extends Part
	Disabled: boolean
	Occupant: Humanoid
	function Sit(self, humanoid: Humanoid): nil
end



declare class SpawnLocation extends Part
	AllowTeamChangeOnTouch: boolean
	Duration: number
	Enabled: boolean
	Neutral: boolean
	TeamColor: BrickColor
end

declare class WedgePart extends FormFactorPart
end

declare class Terrain extends BasePart
	Decoration: boolean
	LastUsedModificationMethod: EnumTerrainAcquisitionMethod
	MaterialColors: BinaryString
	MaxExtents: Region3int16
	ShorelinesUpgraded: boolean
	SmoothVoxelsUpgraded: boolean
	WaterColor: Color3
	WaterReflectance: number
	WaterTransparency: number
	WaterWaveSize: number
	WaterWaveSpeed: number
	function CanShorelinesBeUpgraded(self): boolean
	function CanSmoothVoxelsBeUpgraded(self): boolean
	function CellCenterToWorld(self, x: number, y: number, z: number): Vector3
	function CellCornerToWorld(self, x: number, y: number, z: number): Vector3
	function Clear(self): nil
	function CopyRegion(self, region: Region3int16): TerrainRegion
	function CountCells(self): number
	function FillBall(self, center: Vector3, radius: number, material: EnumMaterial): nil
	function FillBlock(self, cframe: CFrame, size: Vector3, material: EnumMaterial): nil
	function FillCylinder(self, cframe: CFrame, height: number, radius: number, material: EnumMaterial): nil
	function FillRegion(self, region: Region3, resolution: number, material: EnumMaterial): nil
	function FillWedge(self, cframe: CFrame, size: Vector3, material: EnumMaterial): nil
	function GetMaterialColor(self, material: EnumMaterial): Color3
	function GetTerrainWireframe(self, cframe: CFrame, size: Vector3): { any }
	function PasteRegion(self, region: TerrainRegion, corner: Vector3int16, pasteEmptyCells: boolean): nil
	function ReadVoxels(self, region: Region3, resolution: number): any
	function ReplaceMaterial(self, region: Region3, resolution: number, sourceMaterial: EnumMaterial, targetMaterial: EnumMaterial): nil
	function ReplaceMaterialInTransform(self, cframe: CFrame, size: Vector3, sourceMaterial: EnumMaterial, targetMaterial: EnumMaterial): nil
	function ReplaceMaterialInTransformSubregion(self, cframe: CFrame, size: Vector3, sourceMaterial: EnumMaterial, targetMaterial: EnumMaterial, targetRegion: Region3int16): nil
	function SetMaterialColor(self, material: EnumMaterial, value: Color3): nil
	function SetMaterialInTransform(self, cframe: CFrame, size: Vector3, targetMaterial: EnumMaterial): nil
	function SetMaterialInTransformSubregion(self, cframe: CFrame, size: Vector3, targetMaterial: EnumMaterial, targetRegion: Region3int16): nil
	function SmoothRegion(self, region: Region3, resolution: number, strength: number): any
	function WorldToCell(self, position: Vector3): Vector3
	function WorldToCellPreferEmpty(self, position: Vector3): Vector3
	function WorldToCellPreferSolid(self, position: Vector3): Vector3
	function WriteVoxels(self, region: Region3, resolution: number, materials: { any }, occupancy: { any }): nil
end

declare class TriangleMeshPart extends BasePart
	CollisionFidelity: EnumCollisionFidelity
	MeshSize: Vector3
end

declare class MeshPart extends TriangleMeshPart
	DoubleSided: boolean
	HasJointOffset: boolean
	HasSkinnedMesh: boolean
	JointOffset: Vector3
	MeshId: Content
	RenderFidelity: EnumRenderFidelity
	TextureID: Content
	function ApplyMesh(self, meshPart: MeshPart): nil
end

declare class PartOperation extends TriangleMeshPart
	RenderFidelity: EnumRenderFidelity
	SmoothingAngle: number
	TriangleCount: number
	UsePartColor: boolean
	function SubstituteGeometry(self, source: Instance): nil
end

declare class IntersectOperation extends PartOperation
end

declare class NegateOperation extends PartOperation
end

declare class UnionOperation extends PartOperation
end

declare class TrussPart extends BasePart
	Style: EnumStyle
end

declare class VehicleSeat extends BasePart
	AreHingesDetected: number
	Disabled: boolean
	HeadsUpDisplay: boolean
	MaxSpeed: number
	Occupant: Humanoid
	Steer: number
	SteerFloat: number
	Throttle: number
	ThrottleFloat: number
	Torque: number
	TurnSpeed: number
	function Sit(self, humanoid: Humanoid): nil
end

declare class Model extends PVInstance
	LevelOfDetail: EnumModelLevelOfDetail
	ModelStreamingMode: EnumModelStreamingMode
	PrimaryPart: BasePart?
	Scale: number
	WorldPivot: CFrame
	function AddPersistentPlayer(self, playerInstance: Player?): nil
	function GetBoundingBox(self): (CFrame, Vector3)
	function GetExtentsSize(self): Vector3
	function GetPersistentPlayers(self): { Instance }
	function GetScale(self): number
	function MoveTo(self, position: Vector3): nil
	function RemovePersistentPlayer(self, playerInstance: Player?): nil
	function ScaleTo(self, newScaleFactor: number): nil
	function TranslateBy(self, delta: Vector3): nil
end

declare class Actor extends Model
	function BindToMessage(self, topic: string, func: ((...any) -> ...any)): RBXScriptConnection
	function BindToMessageParallel(self, topic: string, func: ((...any) -> ...any)): RBXScriptConnection
	function SendMessage(self, topic: string, ...: any): ()
end

declare class BackpackItem extends Model
	TextureId: Content
end



declare class Tool extends BackpackItem
	Activated: RBXScriptSignal<>
	CanBeDropped: boolean
	Deactivated: RBXScriptSignal<>
	Enabled: boolean
	Equipped: RBXScriptSignal<Mouse>
	Grip: CFrame
	GripForward: Vector3
	GripPos: Vector3
	GripRight: Vector3
	GripUp: Vector3
	ManualActivationOnly: boolean
	RequiresHandle: boolean
	ToolTip: string
	Unequipped: RBXScriptSignal<>
	function Activate(self): nil
	function Deactivate(self): nil
end





declare class WorldRoot extends Model
	function ArePartsTouchingOthers(self, partList: { BasePart }, overlapIgnored: number?): boolean
	function Blockcast(self, cframe: CFrame, size: Vector3, direction: Vector3, params: RaycastParams?): RaycastResult
	function BulkMoveTo(self, partList: { BasePart }, cframeList: { CFrame }, eventMode: EnumBulkMoveMode?): nil
	function CacheCurrentTerrain(self, id: string, center: Vector3, radius: number): string
	function ClearCachedTerrain(self, id: string): boolean
	function GetPartBoundsInBox(self, cframe: CFrame, size: Vector3, overlapParams: OverlapParams?): { BasePart }
	function GetPartBoundsInRadius(self, position: Vector3, radius: number, overlapParams: OverlapParams?): { BasePart }
	function GetPartsInPart(self, part: BasePart, overlapParams: OverlapParams?): { BasePart }
	function IKMoveTo(self, part: BasePart, target: CFrame, translateStiffness: number?, rotateStiffness: number?, collisionsMode: EnumIKCollisionsMode?): nil
	function Raycast(self, origin: Vector3, direction: Vector3, raycastParams: RaycastParams?): RaycastResult?
	function RaycastCachedTerrain(self, id: string, origin: Vector3, direction: Vector3, ignoreWater: boolean): RaycastResult
	function SetInsertPoint(self, point: Vector3, ignoreGrid: boolean?): nil
	function Spherecast(self, position: Vector3, radius: number, direction: Vector3, params: RaycastParams?): RaycastResult
end

declare class Workspace extends WorldRoot
	AirDensity: number
	AllowThirdPartySales: boolean
	AvatarUnificationMode: EnumAvatarUnificationMode
	ClientAnimatorThrottling: EnumClientAnimatorThrottlingMode
	CurrentCamera: Camera
	DistributedGameTime: number
	FallenPartsDestroyHeight: number
	FluidForces: EnumFluidForces
	GlobalWind: Vector3
	Gravity: number
	HumanoidOnlySetCollisionsOnStateChange: EnumHumanoidOnlySetCollisionsOnStateChange
	IKControlConstraintSupport: EnumIKControlConstraintSupport
	InterpolationThrottling: EnumInterpolationThrottlingMode
	MeshPartHeadsAndAccessories: EnumMeshPartHeadsAndAccessories
	ModelStreamingBehavior: EnumModelStreamingBehavior
	PersistentLoaded: RBXScriptSignal<Player>
	PhysicsSteppingMethod: EnumPhysicsSteppingMethod
	RejectCharacterDeletions: EnumRejectCharacterDeletions
	ReplicateInstanceDestroySetting: EnumReplicateInstanceDestroySetting
	Retargeting: EnumAnimatorRetargetingMode
	SignalBehavior: EnumSignalBehavior
	StreamOutBehavior: EnumStreamOutBehavior
	StreamingEnabled: boolean
	StreamingIntegrityMode: EnumStreamingIntegrityMode
	StreamingMinRadius: number
	StreamingTargetRadius: number
	Terrain: Terrain
	TouchesUseCollisionGroups: boolean
	function CalculateJumpDistance(self, gravity: number, jumpPower: number, walkSpeed: number): number
	function CalculateJumpHeight(self, gravity: number, jumpPower: number): number
	function CalculateJumpPower(self, gravity: number, jumpHeight: number): number
	function ExperimentalSolverIsEnabled(self): boolean
	function GetNumAwakeParts(self): number
	function GetPhysicsThrottling(self): number
	function GetRealPhysicsFPS(self): number
	function GetServerTimeNow(self): number
	function JoinToOutsiders(self, objects: { Instance }, jointType: EnumJointCreationMode): nil
	function PGSIsEnabled(self): boolean
	function SetMeshPartHeadsAndAccessories(self, value: EnumMeshPartHeadsAndAccessories): nil
	function SetPhysicsThrottleEnabled(self, value: boolean): nil
	function UnjoinFromOutsiders(self, objects: { Instance }): nil
	function ZoomToExtents(self): nil
end

declare class WorldModel extends WorldRoot
end

declare class PackageLink extends Instance
	AutoUpdate: boolean
	Creator: string
	PackageAssetName: string
	PackageId: Content
	PermissionLevel: EnumPackagePermission
	Status: string
	VersionNumber: number
end

declare class PackageService extends Instance
end

declare class PackageUIService extends Instance
	OnConvertToPackageResult: RBXScriptSignal<boolean, string>
	OnOpenConvertToPackagePlugin: RBXScriptSignal<{ Instance }, string, { Instance }>
	function ConvertToPackageUpload(self, uploadUrl: string, cloneInstances: { Instance }, originalInstances: { Instance }): nil
	function GetPackageInfo(self, packageAssetId: number): { [any]: any }
	function PublishPackage(self, packageInstance: Instance): nil
	function SetPackageVersion(self, packageInstance: Instance, versionNumber: number): Instance
end

declare class Pages extends Instance
	IsFinished: boolean
	function AdvanceToNextPageAsync(self): nil
	function GetCurrentPage(self): { any }
end

declare class AudioPages extends Pages
end

declare class CatalogPages extends Pages
end

declare class DataStoreKeyPages extends Pages
	Cursor: string
end

declare class DataStoreListingPages extends Pages
	Cursor: string
end

declare class DataStorePages extends Pages
end

declare class DataStoreVersionPages extends Pages
end

declare class FriendPages extends Pages
end

declare class InventoryPages extends Pages
end

declare class EmotesPages extends InventoryPages
end

declare class OutfitPages extends Pages
end

declare class StandardPages extends Pages
end

declare class PartOperationAsset extends Instance
end

declare class ParticleEmitter extends Instance
	Acceleration: Vector3
	Brightness: number
	Color: ColorSequence
	Drag: number
	EmissionDirection: EnumNormalId
	Enabled: boolean
	FlipbookFramerate: NumberRange
	FlipbookIncompatible: string
	FlipbookLayout: EnumParticleFlipbookLayout
	FlipbookMode: EnumParticleFlipbookMode
	FlipbookStartRandom: boolean
	Lifetime: NumberRange
	LightEmission: number
	LightInfluence: number
	LockedToPart: boolean
	Orientation: EnumParticleOrientation
	Rate: number
	RotSpeed: NumberRange
	Rotation: NumberRange
	Shape: EnumParticleEmitterShape
	ShapeInOut: EnumParticleEmitterShapeInOut
	ShapePartial: number
	ShapeStyle: EnumParticleEmitterShapeStyle
	Size: NumberSequence
	Speed: NumberRange
	SpreadAngle: Vector2
	Squash: NumberSequence
	Texture: Content
	TimeScale: number
	Transparency: NumberSequence
	VelocityInheritance: number
	WindAffectsDrag: boolean
	ZOffset: number
	function Clear(self): nil
	function Emit(self, particleCount: number?): nil
	function FastForward(self, numFrames: number): nil
end

declare class PatchBundlerFileWatch extends Instance
end

declare class PatchMapping extends Instance
	FlattenTree: boolean
	PatchId: string
	TargetPath: string
end

declare class Path extends Instance
	Blocked: RBXScriptSignal<number>
	Status: EnumPathStatus
	Unblocked: RBXScriptSignal<number>
	function CheckOcclusionAsync(self, start: number): number
	function ComputeAsync(self, start: Vector3, finish: Vector3): nil
	function GetWaypoints(self): { PathWaypoint }
end

declare class PathfindingLink extends Instance
	Attachment0: Attachment
	Attachment1: Attachment
	IsBidirectional: boolean
	Label: string
end

declare class PathfindingModifier extends Instance
	Label: string
	PassThrough: boolean
end

declare class PathfindingService extends Instance
	function CreatePath(self, agentParameters: { [any]: any }?): Path
	function FindPathAsync(self, start: Vector3, finish: Vector3): Path
end

declare class PausedState extends Instance
	AllThreadsPaused: boolean
	Reason: EnumDebuggerPauseReason
	ThreadId: number
end

declare class PausedStateBreakpoint extends PausedState
	Breakpoint: Breakpoint
end

declare class PausedStateException extends PausedState
	ExceptionText: string
end

declare class PermissionsService extends Instance
	function GetIsThirdPartyAssetAllowed(self): boolean
	function GetIsThirdPartyPurchaseAllowed(self): boolean
	function GetIsThirdPartyTeleportAllowed(self): boolean
	function GetPermissions(self, assetId: string): { any }
	function SetPermissions(self, assetId: string, permissions: { any }): nil
end

declare class PhysicsService extends Instance
	function CollisionGroupSetCollidable(self, name1: string, name2: string, collidable: boolean): nil
	function CollisionGroupsAreCollidable(self, name1: string, name2: string): boolean
	function GetMaxCollisionGroups(self): number
	function GetRegisteredCollisionGroups(self): { any }
	function IkSolve(self, part: BasePart, target: CFrame, translateStiffness: number, rotateStiffness: number): nil
	function IsCollisionGroupRegistered(self, name: string): boolean
	function LocalIkSolve(self, part: BasePart, target: CFrame, translateStiffness: number, rotateStiffness: number): nil
	function RegisterCollisionGroup(self, name: string): nil
	function RenameCollisionGroup(self, from: string, to: string): nil
	function UnregisterCollisionGroup(self, name: string): nil
end

declare class PhysicsSettings extends Instance
	AllowSleep: boolean
	AreAnchorsShown: boolean
	AreAssembliesShown: boolean
	AreAwakePartsHighlighted: boolean
	AreBodyTypesShown: boolean
	AreCollisionCostsShown: boolean
	AreConstraintForcesShownForSelectedOrHoveredInstances: boolean
	AreConstraintTorquesShownForSelectedOrHoveredInstances: boolean
	AreContactForcesShownForSelectedOrHoveredAssemblies: boolean
	AreContactIslandsShown: boolean
	AreContactPointsShown: boolean
	AreJointCoordinatesShown: boolean
	AreMagnitudesShownForDrawnForcesAndTorques: boolean
	AreMechanismsShown: boolean
	AreModelCoordsShown: boolean
	AreOwnersShown: boolean
	ArePartCoordsShown: boolean
	AreRegionsShown: boolean
	AreSolverIslandsShown: boolean
	AreTerrainReplicationRegionsShown: boolean
	AreTimestepsShown: boolean
	AreUnalignedPartsShown: boolean
	AreWorldCoordsShown: boolean
	DisableCSGv2: boolean
	DisableCSGv3ForPlugins: boolean
	ForceCSGv2: boolean
	ForceDrawScale: number
	IsInterpolationThrottleShown: boolean
	IsReceiveAgeShown: boolean
	IsTreeShown: boolean
	PhysicsEnvironmentalThrottle: EnumEnviromentalPhysicsThrottle
	ShowDecompositionGeometry: boolean
	SolverConvergenceVisualizationMode: EnumSolverConvergenceVisualizationMode
	ThrottleAdjustTime: number
	UseCSGv2: boolean
end

declare class PlaceStatsService extends Instance
end

declare class PlacesService extends Instance
	function StartPlaySolo(self): nil
end

declare class Player extends Instance
	AccountAge: number
	AutoJumpEnabled: boolean
	CameraMaxZoomDistance: number
	CameraMinZoomDistance: number
	CameraMode: EnumCameraMode
	CanLoadCharacterAppearance: boolean
	Character: Model?
	CharacterAdded: RBXScriptSignal<Model>
	CharacterAppearanceId: number
	CharacterAppearanceLoaded: RBXScriptSignal<Model>
	CharacterRemoving: RBXScriptSignal<Model>
	ChatMode: EnumChatMode
	Chatted: RBXScriptSignal<string, Player?>
	DevCameraOcclusionMode: EnumDevCameraOcclusionMode
	DevComputerCameraMode: EnumDevComputerCameraMovementMode
	DevComputerMovementMode: EnumDevComputerMovementMode
	DevEnableMouseLock: boolean
	DevTouchCameraMode: EnumDevTouchCameraMovementMode
	DevTouchMovementMode: EnumDevTouchMovementMode
	DisplayName: string
	FollowUserId: number
	FriendStatusChanged: RBXScriptSignal<Player, EnumFriendStatus>
	GameplayPaused: boolean
	Guest: boolean
	HasVerifiedBadge: boolean
	HealthDisplayDistance: number
	Idled: RBXScriptSignal<number>
	LocaleId: string
	MaximumSimulationRadius: number
	MembershipType: EnumMembershipType
	NameDisplayDistance: number
	Neutral: boolean
	OnTeleport: RBXScriptSignal<EnumTeleportState, number, string>
	OsPlatform: string
	PlatformName: string
	ReplicationFocus: Instance
	RespawnLocation: SpawnLocation
	SimulationRadius: number
	SimulationRadiusChanged: RBXScriptSignal<number>
	Team: Team
	TeamColor: BrickColor
	Teleported: boolean
	TeleportedIn: boolean
	UnfilteredChat: boolean
	UserId: number
	VRDevice: string
	VREnabled: boolean
	function AddToBlockList(self, userIds: { any }): nil
	function ClearCharacterAppearance(self): nil
	function DistanceFromCharacter(self, point: Vector3): number
	function GetFriendStatus(self, player: Player): EnumFriendStatus
	function GetFriendsOnline(self, maxFriends: number?): { any }
	function GetGameSessionID(self): string
	function GetJoinData(self): { [any]: any }
	function GetMouse(self): Mouse
	function GetNetworkPing(self): number
	function GetRankInGroup(self, groupId: number): number
	function GetRoleInGroup(self, groupId: number): string
	function GetUnder13(self): boolean
	function HasAppearanceLoaded(self): boolean
	function IsFriendsWith(self, userId: number): boolean
	function IsInGroup(self, groupId: number): boolean
	function IsVerified(self): boolean
	function Kick(self, message: string?): nil
	function LoadCharacter(self): nil
	function LoadCharacterBlocking(self): nil
	function LoadCharacterWithHumanoidDescription(self, humanoidDescription: HumanoidDescription): nil
	function Move(self, walkDirection: Vector3, relativeToCamera: boolean?): nil
	function RemoveCharacter(self): nil
	function RequestFriendship(self, player: Player): nil
	function RequestStreamAroundAsync(self, position: Vector3, timeOut: number?): nil
	function RevokeFriendship(self, player: Player): nil
	function SetAccountAge(self, accountAge: number): nil
	function SetCharacterAppearanceJson(self, jsonBlob: string): nil
	function SetExperienceSettingsLocaleId(self, locale: string): nil
	function SetMembershipType(self, membershipType: EnumMembershipType): nil
	function SetModerationAccessKey(self, moderationAccessKey: string): nil
	function SetSuperSafeChat(self, value: boolean): nil
	function UpdatePlayerBlocked(self, userId: number, blocked: boolean): nil
end

declare class PlayerEmulatorService extends Instance
	CustomPoliciesEnabled: boolean
	EmulatedCountryCode: string
	EmulatedGameLocale: string
	PlayerEmulationEnabled: boolean
	SerializedEmulatedPolicyInfo: BinaryString
	function GetEmulatedPolicyInfo(self): { [any]: any }
	function RegionCodeWillHaveAutomaticNonCustomPolicies(self, regionCode: string): boolean
	function SetEmulatedPolicyInfo(self, emulatedPolicyInfo: { [any]: any }): nil
end

declare class PlayerScripts extends Instance
	ComputerCameraMovementModeRegistered: RBXScriptSignal<>
	ComputerMovementModeRegistered: RBXScriptSignal<>
	TouchCameraMovementModeRegistered: RBXScriptSignal<>
	TouchMovementModeRegistered: RBXScriptSignal<>
	function ClearComputerCameraMovementModes(self): nil
	function ClearComputerMovementModes(self): nil
	function ClearTouchCameraMovementModes(self): nil
	function ClearTouchMovementModes(self): nil
	function GetRegisteredComputerCameraMovementModes(self): { any }
	function GetRegisteredComputerMovementModes(self): { any }
	function GetRegisteredTouchCameraMovementModes(self): { any }
	function GetRegisteredTouchMovementModes(self): { any }
	function RegisterComputerCameraMovementMode(self, cameraMovementMode: EnumComputerCameraMovementMode): nil
	function RegisterComputerMovementMode(self, movementMode: EnumComputerMovementMode): nil
	function RegisterTouchCameraMovementMode(self, cameraMovementMode: EnumTouchCameraMovementMode): nil
	function RegisterTouchMovementMode(self, movementMode: EnumTouchMovementMode): nil
end

declare class Players extends Instance
	BubbleChat: boolean
	CharacterAutoLoads: boolean
	ClassicChat: boolean
	FriendRequestEvent: RBXScriptSignal<Player, Player, EnumFriendRequestEvent>
	GameAnnounce: RBXScriptSignal<string>
	LocalPlayer: Player
	MaxPlayers: number
	MaxPlayersInternal: number
	PlayerAdded: RBXScriptSignal<Player>
	PlayerChatted: RBXScriptSignal<EnumPlayerChatType, Player, string, Player?>
	PlayerConnecting: RBXScriptSignal<Player>
	PlayerDisconnecting: RBXScriptSignal<Player>
	PlayerMembershipChanged: RBXScriptSignal<Player>
	PlayerRejoining: RBXScriptSignal<Player>
	PlayerRemoving: RBXScriptSignal<Player>
	PreferredPlayers: number
	PreferredPlayersInternal: number
	RespawnTime: number
	UseStrafingAnimations: boolean
	function Chat(self, message: string): nil
	function CreateHumanoidModelFromDescription(self, description: HumanoidDescription, rigType: EnumHumanoidRigType, assetTypeVerification: EnumAssetTypeVerification?): Model
	function CreateHumanoidModelFromUserId(self, userId: number): Model
	function CreateLocalPlayer(self): Player
	function GetCharacterAppearanceInfoAsync(self, userId: number): { [any]: any }
	function GetFriendsAsync(self, userId: number): FriendPages
	function GetHumanoidDescriptionFromOutfitId(self, outfitId: number): HumanoidDescription
	function GetHumanoidDescriptionFromUserId(self, userId: number): HumanoidDescription
	function GetNameFromUserIdAsync(self, userId: number): string
	function GetPlayerByUserId(self, userId: number): Player?
	function GetPlayerFromCharacter(self, character: Model): Player?
	function GetPlayers(self): { Player }
	function GetUserIdFromNameAsync(self, userName: string): number
	function GetUserThumbnailAsync(self, userId: number, thumbnailType: EnumThumbnailType, thumbnailSize: EnumThumbnailSize): (string, boolean)
	function ReportAbuse(self, player: Player, reason: string, optionalMessage: string): nil
	function ReportAbuseV3(self, player: Player, jsonTags: string): nil
	function ResetLocalPlayer(self): nil
	function SetChatStyle(self, style: EnumChatStyle?): nil
	function SetLocalPlayerInfo(self, userId: number, userName: string, displayName: string, membershipType: EnumMembershipType, isUnder13: boolean): nil
	function TeamChat(self, message: string): nil
	function WhisperChat(self, message: string, player: Instance): nil
end

declare class Plugin extends Instance
	CollisionEnabled: boolean
	Deactivation: RBXScriptSignal<>
	GridSize: number
	HostDataModelType: EnumStudioDataModelType
	HostDataModelTypeIsCurrent: boolean
	MultipleDocumentInterfaceInstance: MultipleDocumentInterfaceInstance
	ProcessAssetInsertionDrag: (assetId: string, assetTypeId: number, instances: { Instance }) -> { Instance }
	ProcessAssetInsertionDrop: () -> nil
	Ready: RBXScriptSignal<>
	Unloading: RBXScriptSignal<>
	UsesAssetInsertionDrag: boolean
	function Activate(self, exclusiveMouse: boolean): nil
	function CreateDockWidgetPluginGui(self, pluginGuiId: string, dockWidgetPluginGuiInfo: DockWidgetPluginGuiInfo): DockWidgetPluginGui
	function CreatePluginAction(self, actionId: string, text: string, statusTip: string, iconName: string?, allowBinding: boolean?): PluginAction
	function CreatePluginMenu(self, id: string, title: string?, icon: string?): PluginMenu
	function CreateQWidgetPluginGui(self, pluginGuiId: string, pluginGuiOptions: { [any]: any }): QWidgetPluginGui
	function CreateToolbar(self, name: string): PluginToolbar
	function Deactivate(self): nil
	function GetItem(self, key: string, defaultValue: any): any
	function GetJoinMode(self): EnumJointCreationMode
	function GetMouse(self): PluginMouse
	function GetSelectedRibbonTool(self): EnumRibbonTool
	function GetSetting(self, key: string): any
	function ImportFbxAnimation(self, rigModel: Instance, isR15: boolean?): Instance
	function ImportFbxRig(self, isR15: boolean?): Instance
	function Intersect(self, objects: { Instance }): Instance
	function Invoke(self, key: string, arguments: any): nil
	function IsActivated(self): boolean
	function IsActivatedWithExclusiveMouse(self): boolean
	function Negate(self, objects: { Instance }): { NegateOperation }
	function OnInvoke(self, key: string, callback: ((...any) -> ...any)): Instance
	function OnSetItem(self, key: string, callback: ((...any) -> ...any)): Instance
	function OpenScript(self, script: BaseScript, lineNumber: number?): nil
	function OpenWikiPage(self, url: string): nil
	function PauseSound(self, sound: Instance): nil
	function PlaySound(self, sound: Instance, normalizedTimePosition: number?): nil
	function PromptForExistingAssetId(self, assetType: string): number
	function PromptSaveSelection(self, suggestedFileName: string?): boolean
	function ResumeSound(self, sound: Instance): nil
	function SaveSelectedToRoblox(self): nil
	function SelectRibbonTool(self, tool: EnumRibbonTool, position: UDim2): nil
	function Separate(self, objects: { Instance }): { UnionOperation }
	function SetItem(self, key: string, value: any): nil
	function SetReady(self): nil
	function SetSetting(self, key: string, value: any): nil
	function StartDecalDrag(self, decal: Instance): nil
	function StartDrag(self, dragData: { [any]: any }): nil
	function StopAllSounds(self): nil
	function Union(self, objects: { Instance }): UnionOperation
end

declare class PluginAction extends Instance
	ActionId: string
	AllowBinding: boolean
	Checked: boolean
	DefaultShortcut: string
	Enabled: boolean
	StatusTip: string
	Text: string
	Triggered: RBXScriptSignal<>
end

declare class PluginCapabilities extends Instance
	Manifest: string
end

declare class PluginDebugService extends Instance
end

declare class PluginDragEvent extends Instance
	Data: string
	MimeType: string
	Position: Vector2
	Sender: string
end

declare class PluginGuiService extends Instance
end

declare class PluginManagementService extends Instance
	function GetOTAPluginVersion(self, pluginName: string?): number
	function SetAutoUpdate(self, pluginId: number, state: boolean): nil
end

declare class PluginManager extends Instance
	function ExportPlace(self, filePath: string?): nil
	function ExportSelection(self, filePath: string?): nil
end

declare class PluginManagerInterface extends Instance
	function ExportPlace(self, filePath: string?): nil
	function ExportSelection(self, filePath: string?): nil
end

declare class PluginMenu extends Instance
	Icon: string
	Title: string
	function AddAction(self, action: PluginAction): nil
	function AddMenu(self, menu: PluginMenu): nil
	function AddNewAction(self, actionId: string, text: string, icon: string?): PluginAction
	function AddSeparator(self): nil
	function Clear(self): nil
	function ShowAsync(self): PluginAction
end

declare class PluginPolicyService extends Instance
	function GetPluginPolicy(self, pluginName: string): { [any]: any }
end

declare class PluginToolbar extends Instance
	function CreateButton(self, id: string, toolTip: string, iconAsset: string, text: string?): PluginToolbarButton
end

declare class PluginToolbarButton extends Instance
	Click: RBXScriptSignal<>
	ClickableWhenViewportHidden: boolean
	Enabled: boolean
	Icon: Content
	function SetActive(self, active: boolean): nil
end



declare class PolicyService extends Instance
	IsLuobuServer: EnumTriStateBoolean
	LuobuWhitelisted: EnumTriStateBoolean
	function GetPolicyInfoForPlayerAsync(self, player: Player): { [any]: any }
	function GetPolicyInfoForServerRobloxOnlyAsync(self): { [any]: any }
end

declare class PoseBase extends Instance
	EasingDirection: EnumPoseEasingDirection
	EasingStyle: EnumPoseEasingStyle
	Weight: number
end

declare class NumberPose extends PoseBase
	Value: number
end

declare class Pose extends PoseBase
	CFrame: CFrame
	function AddSubPose(self, pose: Pose): nil
	function GetSubPoses(self): { Instance }
	function RemoveSubPose(self, pose: Pose): nil
end

declare class PostEffect extends Instance
	Enabled: boolean
end

declare class BloomEffect extends PostEffect
	Intensity: number
	Size: number
	Threshold: number
end

declare class BlurEffect extends PostEffect
	Size: number
end

declare class ColorCorrectionEffect extends PostEffect
	Brightness: number
	Contrast: number
	Saturation: number
	TintColor: Color3
end

declare class DepthOfFieldEffect extends PostEffect
	FarIntensity: number
	FocusDistance: number
	InFocusRadius: number
	NearIntensity: number
end

declare class SunRaysEffect extends PostEffect
	Intensity: number
	Spread: number
end

declare class ProcessInstancePhysicsService extends Instance
end

declare class ProximityPrompt extends Instance
	ActionText: string
	AutoLocalize: boolean
	ClickablePrompt: boolean
	Enabled: boolean
	Exclusivity: EnumProximityPromptExclusivity
	GamepadKeyCode: EnumKeyCode
	HoldDuration: number
	KeyboardKeyCode: EnumKeyCode
	MaxActivationDistance: number
	ObjectText: string
	PromptButtonHoldBegan: RBXScriptSignal<Player>
	PromptButtonHoldEnded: RBXScriptSignal<Player>
	PromptHidden: RBXScriptSignal<>
	PromptShown: RBXScriptSignal<EnumProximityPromptInputType>
	RequiresLineOfSight: boolean
	RootLocalizationTable: LocalizationTable
	Style: EnumProximityPromptStyle
	TriggerEnded: RBXScriptSignal<Player>
	Triggered: RBXScriptSignal<Player>
	UIOffset: Vector2
	function InputHoldBegin(self): nil
	function InputHoldEnd(self): nil
end

declare class ProximityPromptService extends Instance
	Enabled: boolean
	MaxPromptsVisible: number
	PromptButtonHoldBegan: RBXScriptSignal<ProximityPrompt, Player>
	PromptButtonHoldEnded: RBXScriptSignal<ProximityPrompt, Player>
	PromptHidden: RBXScriptSignal<ProximityPrompt>
	PromptShown: RBXScriptSignal<ProximityPrompt, EnumProximityPromptInputType>
	PromptTriggerEnded: RBXScriptSignal<ProximityPrompt, Player>
	PromptTriggered: RBXScriptSignal<ProximityPrompt, Player>
end

declare class PublishService extends Instance
	function CreateAssetAndWaitForAssetId(self, instances: { Instance }, operationId: string, creatorId: number, assetType: string, name: string, description: string): number
	function PublishCageMeshAsync(self, wrap: Instance, cageType: EnumCageType): Content
	function PublishDescendantAssets(self, instance: Instance): boolean
end

declare class RbxAnalyticsService extends Instance
	function AddGlobalPointsField(self, key: string, value: number): nil
	function AddGlobalPointsTag(self, key: string, value: string): nil
	function DEPRECATED_TrackEvent(self, category: string, action: string, label: string, value: number?): nil
	function DEPRECATED_TrackEventWithArgs(self, category: string, action: string, label: string, args: { [any]: any }, value: number?): nil
	function GetClientId(self): string
	function GetPlaySessionId(self): string
	function GetSessionId(self): string
	function ReleaseRBXEventStream(self, target: string): nil
	function RemoveGlobalPointsField(self, key: string): nil
	function RemoveGlobalPointsTag(self, key: string): nil
	function ReportCounter(self, counterName: string, amount: number?): nil
	function ReportInfluxSeries(self, seriesName: string, points: { [any]: any }, throttlingPercentage: number): nil
	function ReportStats(self, category: string, value: number): nil
	function ReportToDiagByCountryCode(self, featureName: string, measureName: string, seconds: number): nil
	function SendEventDeferred(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function SendEventImmediately(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function SetRBXEvent(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function SetRBXEventStream(self, target: string, eventContext: string, eventName: string, additionalArgs: { [any]: any }): nil
	function TrackEvent(self, category: string, action: string, label: string, value: number?): nil
	function TrackEventWithArgs(self, category: string, action: string, label: string, args: { [any]: any }, value: number?): nil
	function UpdateHeartbeatObject(self, args: { [any]: any }): nil
end

declare class ReflectionMetadata extends Instance
end

declare class ReflectionMetadataCallbacks extends Instance
end

declare class ReflectionMetadataClasses extends Instance
end

declare class ReflectionMetadataEnums extends Instance
end

declare class ReflectionMetadataEvents extends Instance
end

declare class ReflectionMetadataFunctions extends Instance
end

declare class ReflectionMetadataItem extends Instance
	Browsable: boolean
	ClassCategory: string
	ClientOnly: boolean
	Constraint: string
	Deprecated: boolean
	EditingDisabled: boolean
	EditorType: string
	FFlag: string
	IsBackend: boolean
	PropertyOrder: number
	ScriptContext: string
	ServerOnly: boolean
	SliderScaling: string
	UIMaximum: number
	UIMinimum: number
	UINumTicks: number
end

declare class ReflectionMetadataClass extends ReflectionMetadataItem
	ExplorerImageIndex: number
	ExplorerOrder: number
	Insertable: boolean
	PreferredParent: string
	ServiceVisibility: EnumServiceVisibility
end

declare class ReflectionMetadataEnum extends ReflectionMetadataItem
end

declare class ReflectionMetadataEnumItem extends ReflectionMetadataItem
end

declare class ReflectionMetadataMember extends ReflectionMetadataItem
end

declare class ReflectionMetadataProperties extends Instance
end

declare class ReflectionMetadataYieldFunctions extends Instance
end

declare class RemoteCursorService extends Instance
end

declare class RemoteDebuggerServer extends Instance
end

declare class RemoteEvent extends Instance
	OnClientEvent: RBXScriptSignal<...any>
	OnServerEvent: RBXScriptSignal<(Player, ...any)>
	function FireAllClients(self, ...: any): ()
	function FireClient(self, player: Player, ...: any): ()
	function FireServer(self, ...: any): ()
end

declare class RemoteFunction extends Instance
	OnClientInvoke: (...any) -> ...any
	OnServerInvoke: (player: Player, ...any) -> ...any
	function InvokeClient(self, player: Player, ...: any): ...any
	function InvokeServer(self, ...: any): ...any
end

declare class RenderSettings extends Instance
	AutoFRMLevel: number
	EagerBulkExecution: boolean
	EditQualityLevel: EnumQualityLevel
	EnableFRM: boolean
	ExportMergeByMaterial: boolean
	FrameRateManager: EnumFramerateManagerMode
	GraphicsMode: EnumGraphicsMode
	MeshCacheSize: number
	MeshPartDetailLevel: EnumMeshPartDetailLevel
	QualityLevel: EnumQualityLevel
	ReloadAssets: boolean
	RenderCSGTrianglesDebug: boolean
	ShowBoundingBoxes: boolean
	ViewMode: EnumViewMode
	function GetMaxQualityLevel(self): number
end

declare class RenderingTest extends Instance
	CFrame: CFrame
	ComparisonDiffThreshold: number
	ComparisonMethod: EnumRenderingTestComparisonMethod
	ComparisonPsnrThreshold: number
	Description: string
	FieldOfView: number
	Orientation: Vector3
	PerfTest: boolean
	Position: Vector3
	QualityLevel: number
	ShouldSkip: boolean
	Ticket: string
	Timeout: number
	function RenderdocTriggerCapture(self): nil
end

declare class ReplicatedFirst extends Instance
	DefaultLoadingGuiRemoved: RBXScriptSignal<>
	FinishedReplicating: RBXScriptSignal<>
	RemoveDefaultLoadingGuiSignal: RBXScriptSignal<>
	function IsDefaultLoadingGuiRemoved(self): boolean
	function IsFinishedReplicating(self): boolean
	function RemoveDefaultLoadingScreen(self): nil
	function SetDefaultLoadingGuiRemoved(self): nil
end

declare class ReplicatedStorage extends Instance
end

declare class RobloxPluginGuiService extends Instance
end

declare class RobloxReplicatedStorage extends Instance
end

declare class RobloxServerStorage extends Instance
end

declare class RomarkService extends Instance
	function EndRemoteRomarkTest(self): nil
end

declare class RotationCurve extends Instance
	Length: number
	function GetKeyAtIndex(self, index: number): RotationCurveKey
	function GetKeyIndicesAtTime(self, time: number): { any }
	function GetKeys(self): { any }
	function GetValueAtTime(self, time: number): CFrame?
	function InsertKey(self, key: RotationCurveKey): { any }
	function RemoveKeyAtIndex(self, startingIndex: number, count: number?): number
	function SetKeys(self, keys: { any }): number
end

declare class RtMessagingService extends Instance
end

declare class RunService extends Instance
	ClientGitHash: string
	Heartbeat: RBXScriptSignal<number>
	PostSimulation: RBXScriptSignal<number>
	PreAnimation: RBXScriptSignal<number>
	PreRender: RBXScriptSignal<number>
	PreSimulation: RBXScriptSignal<number>
	RenderStepped: RBXScriptSignal<number>
	Stepped: RBXScriptSignal<number, number>
	function BindToRenderStep(self, name: string, priority: number, func: ((delta: number) -> ())): ()
	function GetCoreScriptVersion(self): string
	function GetRobloxClientChannel(self): string
	function GetRobloxVersion(self): string
	function IsClient(self): boolean
	function IsEdit(self): boolean
	function IsRunMode(self): boolean
	function IsRunning(self): boolean
	function IsServer(self): boolean
	function IsStudio(self): boolean
	function Pause(self): nil
	function Run(self): nil
	function Set3dRenderingEnabled(self, enable: boolean): nil
	function SetRobloxGuiFocused(self, focus: boolean): nil
	function Stop(self): nil
	function UnbindFromRenderStep(self, name: string): nil
	function setThrottleFramerateEnabled(self, enable: boolean): nil
end

declare class RuntimeScriptService extends Instance
end

declare class SafetyService extends Instance
	ScreenshotContentReady: RBXScriptSignal<number, Content>
	ScreenshotUploaded: RBXScriptSignal<number, string>
	function TakeScreenshot(self, screenshotOptions: { [any]: any }): number
end

declare class ScreenshotHud extends Instance
	CameraButtonIcon: Content
	CameraButtonPosition: UDim2
	CloseButtonPosition: UDim2
	CloseWhenScreenshotTaken: boolean
	ExperienceNameOverlayEnabled: boolean
	OverlayFont: EnumFont
	UsernameOverlayEnabled: boolean
	Visible: boolean
end

declare class ScriptBuilder extends Instance
end

declare class SyncScriptBuilder extends ScriptBuilder
	CompileTarget: EnumCompileTarget
	CoverageInfo: boolean
	DebugInfo: boolean
	PackAsSource: boolean
end

declare class ScriptChangeService extends Instance
	ScriptAdded: RBXScriptSignal<LuaSourceContainer>
	ScriptBeingRemoved: RBXScriptSignal<LuaSourceContainer>
	ScriptChanged: RBXScriptSignal<LuaSourceContainer, string>
	ScriptFullNameChanged: RBXScriptSignal<LuaSourceContainer>
	ScriptSourceChanged: RBXScriptSignal<LuaSourceContainer>
end

declare class ScriptCloneWatcher extends Instance
end

declare class ScriptCloneWatcherHelper extends Instance
end

declare class ScriptCommitService extends Instance
end

declare class ScriptContext extends Instance
	Error: RBXScriptSignal<string, string, Instance>
	ErrorDetailed: RBXScriptSignal<string, string, Instance, string, number>
	ScriptsDisabled: boolean
	function AddCoreScriptLocal(self, name: string, parent: Instance): nil
	function ClearScriptProfilingData(self): nil
	function DeserializeScriptProfilerString(self, jsonString: string): { [any]: any }
	function GetCoverageStats(self): { any }
	function SaveScriptProfilingData(self, filename: string): nil
	function SetTimeout(self, seconds: number): nil
	function StartScriptProfiling(self, frequency: number?): nil
	function StopScriptProfiling(self): string
end

declare class ScriptDebugger extends Instance
	BreakpointAdded: RBXScriptSignal<Instance>
	BreakpointRemoved: RBXScriptSignal<Instance>
	CurrentLine: number
	EncounteredBreak: RBXScriptSignal<number, EnumBreakReason>
	IsDebugging: boolean
	IsPaused: boolean
	Resuming: RBXScriptSignal<>
	Script: Instance
	WatchAdded: RBXScriptSignal<Instance>
	WatchRemoved: RBXScriptSignal<Instance>
	function AddWatch(self, expression: string): Instance
	function GetBreakpoints(self): { Instance }
	function GetGlobals(self, stackFrame: number?): { [any]: any }
	function GetLocals(self, stackFrame: number?): { [any]: any }
	function GetStack(self): { any }
	function GetUpvalues(self, stackFrame: number?): { [any]: any }
	function GetWatchValue(self, watch: Instance): any
	function GetWatches(self): { Instance }
	function SetBreakpoint(self, line: number, isContextDependentBreakpoint: boolean): Instance
	function SetGlobal(self, name: string, value: any, stackFrame: number): nil
	function SetLocal(self, name: string, value: any, stackFrame: number?): nil
	function SetUpvalue(self, name: string, value: any, stackFrame: number?): nil
end

declare class ScriptDocument extends Instance
	SelectionChanged: RBXScriptSignal<number, number, number, number>
	ViewportChanged: RBXScriptSignal<number, number>
	function CloseAsync(self): any
	function EditTextAsync(self, newText: string, startLine: number, startCharacter: number, endLine: number, endCharacter: number): any
	function ForceSetSelectionAsync(self, cursorLine: number, cursorCharacter: number, anchorLine: number?, anchorCharacter: number?): any
	function GetInternalUri(self): string
	function GetLine(self, lineIndex: number?): string
	function GetLineCount(self): number
	function GetScript(self): LuaSourceContainer
	function GetSelectedText(self): string
	function GetSelection(self): any
	function GetSelectionEnd(self): any
	function GetSelectionStart(self): any
	function GetText(self, startLine: number?, startCharacter: number?, endLine: number?, endCharacter: number?): string
	function GetViewport(self): any
	function HasSelectedText(self): boolean
	function IsCommandBar(self): boolean
	function RequestSetSelectionAsync(self, cursorLine: number, cursorCharacter: number, anchorLine: number?, anchorCharacter: number?): any
end

declare class ScriptEditorService extends Instance
	TextDocumentDidChange: RBXScriptSignal<ScriptDocument, any>
	TextDocumentDidClose: RBXScriptSignal<ScriptDocument>
	TextDocumentDidOpen: RBXScriptSignal<ScriptDocument>
	function DeregisterAutocompleteCallback(self, name: string): nil
	function DeregisterScriptAnalysisCallback(self, name: string): nil
	function FindScriptDocument(self, script: LuaSourceContainer): ScriptDocument
	function ForceReloadSource(self, uri: string, newsrc: string): nil
	function GetEditorSource(self, script: LuaSourceContainer): string
	function GetScriptDocuments(self): { Instance }
	function OpenScriptDocumentAsync(self, script: LuaSourceContainer): any
	function RegisterAutocompleteCallback(self, name: string, priority: number, callbackFunction: ((...any) -> ...any)): nil
	function RegisterScriptAnalysisCallback(self, name: string, priority: number, callbackFunction: ((...any) -> ...any)): nil
end

declare class ScriptRegistrationService extends Instance
	function GetSourceContainerByScriptGuid(self, guid: string): LuaSourceContainer
end

declare class ScriptRuntime extends Instance
end

declare class ScriptService extends Instance
end

declare class Selection extends Instance
	ActiveInstance: Instance
	RenderMode: EnumSelectionRenderMode
	SelectionBoxThickness: number
	SelectionChanged: RBXScriptSignal<>
	SelectionLineThickness: number
	SelectionThickness: number
	ShowActiveInstanceHighlight: boolean
	function Add(self, instancesToAdd: { Instance }): nil
	function ClearTerrainSelectionHack(self): nil
	function Get(self): { Instance }
	function Remove(self, instancesToRemove: { Instance }): nil
	function Set(self, selection: { Instance }): nil
	function SetTerrainSelectionHack(self, center: Vector3, size: Vector3): nil
end

declare class SelectionHighlightManager extends Instance
end

declare class SensorBase extends Instance
	OnSensorOutputChanged: RBXScriptSignal<>
	UpdateType: EnumSensorUpdateType
end

declare class BuoyancySensor extends SensorBase
	FullySubmerged: boolean
	TouchingSurface: boolean
end

declare class ControllerSensor extends SensorBase
end

declare class ControllerPartSensor extends ControllerSensor
	HitFrame: CFrame
	HitNormal: Vector3
	SearchDistance: number
	SensedPart: BasePart
	SensorMode: EnumSensorMode
end

declare class ServerScriptService extends Instance
	LoadStringEnabled: boolean
end

declare class ServerStorage extends Instance
end

declare class ServiceProvider extends Instance
	Close: RBXScriptSignal<>
	CloseLate: RBXScriptSignal<>
	ServiceAdded: RBXScriptSignal<Instance>
	ServiceRemoving: RBXScriptSignal<Instance>
	function FindService(self, className: string): Instance
	function GetService(self, service: "AdService"): AdService
	function GetService(self, service: "AnalyticsService"): AnalyticsService
	function GetService(self, service: "AnimationClipProvider"): AnimationClipProvider
	function GetService(self, service: "AnimationFromVideoCreatorService"): AnimationFromVideoCreatorService
	function GetService(self, service: "AnimationFromVideoCreatorStudioService"): AnimationFromVideoCreatorStudioService
	function GetService(self, service: "AppStorageService"): AppStorageService
	function GetService(self, service: "AppUpdateService"): AppUpdateService
	function GetService(self, service: "AssetCounterService"): AssetCounterService
	function GetService(self, service: "AssetDeliveryProxy"): AssetDeliveryProxy
	function GetService(self, service: "AssetImportService"): AssetImportService
	function GetService(self, service: "AssetManagerService"): AssetManagerService
	function GetService(self, service: "AssetService"): AssetService
	function GetService(self, service: "AvatarChatService"): AvatarChatService
	function GetService(self, service: "AvatarEditorService"): AvatarEditorService
	function GetService(self, service: "AvatarImportService"): AvatarImportService
	function GetService(self, service: "BadgeService"): BadgeService
	function GetService(self, service: "BrowserService"): BrowserService
	function GetService(self, service: "BulkImportService"): BulkImportService
	function GetService(self, service: "CSGDictionaryService"): CSGDictionaryService
	function GetService(self, service: "CacheableContentProvider"): CacheableContentProvider
	function GetService(self, service: "CalloutService"): CalloutService
	function GetService(self, service: "CaptureService"): CaptureService
	function GetService(self, service: "ChangeHistoryService"): ChangeHistoryService
	function GetService(self, service: "Chat"): Chat
	function GetService(self, service: "ClusterPacketCache"): ClusterPacketCache
	function GetService(self, service: "CollectionService"): CollectionService
	function GetService(self, service: "CommandService"): CommandService
	function GetService(self, service: "ConfigureServerService"): ConfigureServerService
	function GetService(self, service: "ContentProvider"): ContentProvider
	function GetService(self, service: "ContextActionService"): ContextActionService
	function GetService(self, service: "ControllerService"): ControllerService
	function GetService(self, service: "CookiesService"): CookiesService
	function GetService(self, service: "CoreGui"): CoreGui
	function GetService(self, service: "CorePackages"): CorePackages
	function GetService(self, service: "CoreScriptDebuggingManagerHelper"): CoreScriptDebuggingManagerHelper
	function GetService(self, service: "CoreScriptSyncService"): CoreScriptSyncService
	function GetService(self, service: "CrossDMScriptChangeListener"): CrossDMScriptChangeListener
	function GetService(self, service: "DataModelPatchService"): DataModelPatchService
	function GetService(self, service: "DataStoreService"): DataStoreService
	function GetService(self, service: "Debris"): Debris
	function GetService(self, service: "DebuggablePluginWatcher"): DebuggablePluginWatcher
	function GetService(self, service: "DebuggerConnectionManager"): DebuggerConnectionManager
	function GetService(self, service: "DebuggerManager"): DebuggerManager
	function GetService(self, service: "DebuggerUIService"): DebuggerUIService
	function GetService(self, service: "DeviceIdService"): DeviceIdService
	function GetService(self, service: "DraftsService"): DraftsService
	function GetService(self, service: "DraggerService"): DraggerService
	function GetService(self, service: "EventIngestService"): EventIngestService
	function GetService(self, service: "ExperienceAuthService"): ExperienceAuthService
	function GetService(self, service: "FaceAnimatorService"): FaceAnimatorService
	function GetService(self, service: "FacialAnimationRecordingService"): FacialAnimationRecordingService
	function GetService(self, service: "FacialAnimationStreamingServiceV2"): FacialAnimationStreamingServiceV2
	function GetService(self, service: "FlagStandService"): FlagStandService
	function GetService(self, service: "FlyweightService"): FlyweightService
	function GetService(self, service: "FriendService"): FriendService
	function GetService(self, service: "GamePassService"): GamePassService
	function GetService(self, service: "GamepadService"): GamepadService
	function GetService(self, service: "Geometry"): Geometry
	function GetService(self, service: "GeometryService"): GeometryService
	function GetService(self, service: "GoogleAnalyticsConfiguration"): GoogleAnalyticsConfiguration
	function GetService(self, service: "GroupService"): GroupService
	function GetService(self, service: "GuiService"): GuiService
	function GetService(self, service: "GuidRegistryService"): GuidRegistryService
	function GetService(self, service: "HSRDataContentProvider"): HSRDataContentProvider
	function GetService(self, service: "HapticService"): HapticService
	function GetService(self, service: "HeightmapImporterService"): HeightmapImporterService
	function GetService(self, service: "Hopper"): Hopper
	function GetService(self, service: "HttpRbxApiService"): HttpRbxApiService
	function GetService(self, service: "HttpService"): HttpService
	function GetService(self, service: "ILegacyStudioBridge"): ILegacyStudioBridge
	function GetService(self, service: "IXPService"): IXPService
	function GetService(self, service: "IncrementalPatchBuilder"): IncrementalPatchBuilder
	function GetService(self, service: "InsertService"): InsertService
	function GetService(self, service: "JointsService"): JointsService
	function GetService(self, service: "KeyboardService"): KeyboardService
	function GetService(self, service: "KeyframeSequenceProvider"): KeyframeSequenceProvider
	function GetService(self, service: "LSPFileSyncService"): LSPFileSyncService
	function GetService(self, service: "LanguageService"): LanguageService
	function GetService(self, service: "LegacyStudioBridge"): LegacyStudioBridge
	function GetService(self, service: "Lighting"): Lighting
	function GetService(self, service: "LiveScriptingService"): LiveScriptingService
	function GetService(self, service: "LocalStorageService"): LocalStorageService
	function GetService(self, service: "LocalizationService"): LocalizationService
	function GetService(self, service: "LodDataService"): LodDataService
	function GetService(self, service: "LogService"): LogService
	function GetService(self, service: "LoginService"): LoginService
	function GetService(self, service: "LuaWebService"): LuaWebService
	function GetService(self, service: "LuauScriptAnalyzerService"): LuauScriptAnalyzerService
	function GetService(self, service: "MarketplaceService"): MarketplaceService
	function GetService(self, service: "MaterialGenerationService"): MaterialGenerationService
	function GetService(self, service: "MaterialService"): MaterialService
	function GetService(self, service: "MemStorageService"): MemStorageService
	function GetService(self, service: "MemoryStoreService"): MemoryStoreService
	function GetService(self, service: "MeshContentProvider"): MeshContentProvider
	function GetService(self, service: "MessageBusService"): MessageBusService
	function GetService(self, service: "MessagingService"): MessagingService
	function GetService(self, service: "MetaBreakpointManager"): MetaBreakpointManager
	function GetService(self, service: "MouseService"): MouseService
	function GetService(self, service: "NetworkClient"): NetworkClient
	function GetService(self, service: "NetworkServer"): NetworkServer
	function GetService(self, service: "NetworkSettings"): NetworkSettings
	function GetService(self, service: "NonReplicatedCSGDictionaryService"): NonReplicatedCSGDictionaryService
	function GetService(self, service: "NotificationService"): NotificationService
	function GetService(self, service: "OmniRecommendationsService"): OmniRecommendationsService
	function GetService(self, service: "OpenCloudService"): OpenCloudService
	function GetService(self, service: "PackageService"): PackageService
	function GetService(self, service: "PackageUIService"): PackageUIService
	function GetService(self, service: "PatchBundlerFileWatch"): PatchBundlerFileWatch
	function GetService(self, service: "PathfindingService"): PathfindingService
	function GetService(self, service: "PermissionsService"): PermissionsService
	function GetService(self, service: "PhysicsService"): PhysicsService
	function GetService(self, service: "PlaceStatsService"): PlaceStatsService
	function GetService(self, service: "PlacesService"): PlacesService
	function GetService(self, service: "PlayerEmulatorService"): PlayerEmulatorService
	function GetService(self, service: "Players"): Players
	function GetService(self, service: "PluginDebugService"): PluginDebugService
	function GetService(self, service: "PluginGuiService"): PluginGuiService
	function GetService(self, service: "PluginManagementService"): PluginManagementService
	function GetService(self, service: "PluginPolicyService"): PluginPolicyService
	function GetService(self, service: "PointsService"): PointsService
	function GetService(self, service: "PolicyService"): PolicyService
	function GetService(self, service: "ProcessInstancePhysicsService"): ProcessInstancePhysicsService
	function GetService(self, service: "ProximityPromptService"): ProximityPromptService
	function GetService(self, service: "PublishService"): PublishService
	function GetService(self, service: "RbxAnalyticsService"): RbxAnalyticsService
	function GetService(self, service: "RemoteCursorService"): RemoteCursorService
	function GetService(self, service: "RemoteDebuggerServer"): RemoteDebuggerServer
	function GetService(self, service: "RenderSettings"): RenderSettings
	function GetService(self, service: "ReplicatedFirst"): ReplicatedFirst
	function GetService(self, service: "ReplicatedStorage"): ReplicatedStorage
	function GetService(self, service: "RobloxPluginGuiService"): RobloxPluginGuiService
	function GetService(self, service: "RobloxReplicatedStorage"): RobloxReplicatedStorage
	function GetService(self, service: "RobloxServerStorage"): RobloxServerStorage
	function GetService(self, service: "RomarkService"): RomarkService
	function GetService(self, service: "RtMessagingService"): RtMessagingService
	function GetService(self, service: "RunService"): RunService
	function GetService(self, service: "RuntimeScriptService"): RuntimeScriptService
	function GetService(self, service: "SafetyService"): SafetyService
	function GetService(self, service: "ScriptChangeService"): ScriptChangeService
	function GetService(self, service: "ScriptCloneWatcher"): ScriptCloneWatcher
	function GetService(self, service: "ScriptCloneWatcherHelper"): ScriptCloneWatcherHelper
	function GetService(self, service: "ScriptCommitService"): ScriptCommitService
	function GetService(self, service: "ScriptContext"): ScriptContext
	function GetService(self, service: "ScriptEditorService"): ScriptEditorService
	function GetService(self, service: "ScriptRegistrationService"): ScriptRegistrationService
	function GetService(self, service: "ScriptService"): ScriptService
	function GetService(self, service: "Selection"): Selection
	function GetService(self, service: "SelectionHighlightManager"): SelectionHighlightManager
	function GetService(self, service: "ServerScriptService"): ServerScriptService
	function GetService(self, service: "ServerStorage"): ServerStorage
	function GetService(self, service: "ServiceVisibilityService"): ServiceVisibilityService
	function GetService(self, service: "SessionService"): SessionService
	function GetService(self, service: "SharedTableRegistry"): SharedTableRegistry
	function GetService(self, service: "ShorelineUpgraderService"): ShorelineUpgraderService
	function GetService(self, service: "SmoothVoxelsUpgraderService"): SmoothVoxelsUpgraderService
	function GetService(self, service: "SnippetService"): SnippetService
	function GetService(self, service: "SocialService"): SocialService
	function GetService(self, service: "SolidModelContentProvider"): SolidModelContentProvider
	function GetService(self, service: "SoundService"): SoundService
	function GetService(self, service: "SpawnerService"): SpawnerService
	function GetService(self, service: "StarterGui"): StarterGui
	function GetService(self, service: "StarterPack"): StarterPack
	function GetService(self, service: "StarterPlayer"): StarterPlayer
	function GetService(self, service: "Stats"): Stats
	function GetService(self, service: "StopWatchReporter"): StopWatchReporter
	function GetService(self, service: "Studio"): Studio
	function GetService(self, service: "StudioAssetService"): StudioAssetService
	function GetService(self, service: "StudioData"): StudioData
	function GetService(self, service: "StudioDeviceEmulatorService"): StudioDeviceEmulatorService
	function GetService(self, service: "StudioPublishService"): StudioPublishService
	function GetService(self, service: "StudioScriptDebugEventListener"): StudioScriptDebugEventListener
	function GetService(self, service: "StudioSdkService"): StudioSdkService
	function GetService(self, service: "StudioService"): StudioService
	function GetService(self, service: "StylingService"): StylingService
	function GetService(self, service: "TaskScheduler"): TaskScheduler
	function GetService(self, service: "TeamCreateData"): TeamCreateData
	function GetService(self, service: "TeamCreatePublishService"): TeamCreatePublishService
	function GetService(self, service: "TeamCreateService"): TeamCreateService
	function GetService(self, service: "Teams"): Teams
	function GetService(self, service: "TeleportService"): TeleportService
	function GetService(self, service: "TemporaryCageMeshProvider"): TemporaryCageMeshProvider
	function GetService(self, service: "TemporaryScriptService"): TemporaryScriptService
	function GetService(self, service: "TestService"): TestService
	function GetService(self, service: "TextBoxService"): TextBoxService
	function GetService(self, service: "TextChatService"): TextChatService
	function GetService(self, service: "TextService"): TextService
	function GetService(self, service: "ThirdPartyUserService"): ThirdPartyUserService
	function GetService(self, service: "TimerService"): TimerService
	function GetService(self, service: "ToastNotificationService"): ToastNotificationService
	function GetService(self, service: "TouchInputService"): TouchInputService
	function GetService(self, service: "TracerService"): TracerService
	function GetService(self, service: "TutorialService"): TutorialService
	function GetService(self, service: "TweenService"): TweenService
	function GetService(self, service: "UGCAvatarService"): UGCAvatarService
	function GetService(self, service: "UGCValidationService"): UGCValidationService
	function GetService(self, service: "UnvalidatedAssetService"): UnvalidatedAssetService
	function GetService(self, service: "UserInputService"): UserInputService
	function GetService(self, service: "UserService"): UserService
	function GetService(self, service: "UserStorageService"): UserStorageService
	function GetService(self, service: "VRService"): VRService
	function GetService(self, service: "VRStatusService"): VRStatusService
	function GetService(self, service: "VersionControlService"): VersionControlService
	function GetService(self, service: "VideoCaptureService"): VideoCaptureService
	function GetService(self, service: "VideoService"): VideoService
	function GetService(self, service: "VirtualInputManager"): VirtualInputManager
	function GetService(self, service: "VirtualUser"): VirtualUser
	function GetService(self, service: "VisibilityCheckDispatcher"): VisibilityCheckDispatcher
	function GetService(self, service: "VisibilityService"): VisibilityService
	function GetService(self, service: "Visit"): Visit
	function GetService(self, service: "VoiceChatInternal"): VoiceChatInternal
	function GetService(self, service: "VoiceChatService"): VoiceChatService
	function GetService(self, service: "Workspace"): Workspace
end

declare class DataModel extends ServiceProvider
	CreatorId: number
	CreatorType: EnumCreatorType
	GameId: number
	Genre: EnumGenre
	GraphicsQualityChangeRequest: RBXScriptSignal<boolean>
	IsSFFlagsLoaded: boolean
	JobId: string
	Loaded: RBXScriptSignal<>
	PlaceId: number
	PlaceVersion: number
	PrivateServerId: string
	PrivateServerOwnerId: number
	ScreenshotReady: RBXScriptSignal<string>
	ScreenshotSavedToAlbum: RBXScriptSignal<string, boolean, string>
	UniverseMetadataLoaded: RBXScriptSignal<>
	Workspace: Workspace
	function BindToClose(self, func: ((...any) -> ...any)): nil
	function DefineFastFlag(self, name: string, defaultValue: boolean): boolean
	function DefineFastInt(self, name: string, defaultValue: number): number
	function DefineFastString(self, name: string, defaultValue: string): string
	function GetEngineFeature(self, name: string): boolean
	function GetFastFlag(self, name: string): boolean
	function GetFastInt(self, name: string): number
	function GetFastString(self, name: string): string
	function GetJobsInfo(self): { any }
	function GetObjects(self, url: Content): { Instance }
	function GetObjectsAllOrNone(self, url: Content, binaryFormatOnly: boolean?): { Instance }
	function GetObjectsAsync(self, url: Content): { Instance }
	function GetObjectsList(self, urls: { any }): { any }
	function GetPlaySessionId(self): string
	function HttpGetAsync(self, url: string, httpRequestType: EnumHttpRequestType?): string
	function HttpPostAsync(self, url: string, data: string, contentType: string?, httpRequestType: EnumHttpRequestType?): string
	function InsertObjectsAndJoinIfLegacyAsync(self, url: Content): { Instance }
	function IsContentLoaded(self): boolean
	function IsLoaded(self): boolean
	function IsUniverseMetadataLoaded(self): boolean
	function Load(self, url: Content): nil
	function OpenScreenshotsFolder(self): nil
	function OpenVideosFolder(self): nil
	function ReportInGoogleAnalytics(self, category: string, action: string?, label: string?, value: number?): nil
	function SetFastFlagForTesting(self, name: string, newValue: boolean): boolean
	function SetFastIntForTesting(self, name: string, newValue: number): number
	function SetFastStringForTesting(self, name: string, newValue: string): string
	function SetFlagVersion(self, name: string, version: number): nil
	function SetIsLoaded(self, value: boolean, placeSizeInBytes: number?): nil
	function SetPlaceId(self, placeId: number): nil
	function SetUniverseId(self, universeId: number): nil
	function Shutdown(self): nil
end

declare class GenericSettings extends ServiceProvider
end

declare class AnalysticsSettings extends GenericSettings
end

declare class UserSettings extends GenericSettings
	GameSettings: UserGameSettings
	function GetService(self, service: "UserGameSettings"): UserGameSettings
	function IsUserFeatureEnabled(self, name: string): boolean
	function Reset(self): nil
	function SaveState(self): nil
end

declare class ServiceVisibilityService extends Instance
	HiddenServices: BinaryString
	VisibleServices: BinaryString
end

declare class SessionService extends Instance
	function GetCreatedTimestampUtcMs(self, sid: string): number
	function GetMetadata(self, sid: string, key: string): any
	function GetRootSID(self): string
	function RemoveMetadata(self, sid: string, key: string): nil
	function RemoveSession(self, sid: string): nil
	function RemoveSessionsWithMetadataKey(self, key: string): nil
	function ReplaceSession(self, sid: string, tag: string): nil
	function SessionExists(self, sid: string): boolean
	function SetMetadata(self, sid: string, key: string, value: any): nil
	function SetSession(self, parentSid: string, childSid: string, tag: string): nil
end

declare class SharedTableRegistry extends Instance
	function GetSharedTable(self, name: string): SharedTable
	function SetSharedTable(self, name: string, st: SharedTable?): nil
end

declare class ShorelineUpgraderService extends Instance
	Status: RBXScriptSignal<number>
	function Cancel(self): nil
	function Start(self): nil
end

declare class Sky extends Instance
	CelestialBodiesShown: boolean
	MoonAngularSize: number
	MoonTextureId: Content
	SkyboxBk: Content
	SkyboxDn: Content
	SkyboxFt: Content
	SkyboxLf: Content
	SkyboxRt: Content
	SkyboxUp: Content
	StarCount: number
	SunAngularSize: number
	SunTextureId: Content
end

declare class Smoke extends Instance
	Color: Color3
	Enabled: boolean
	Opacity: number
	RiseVelocity: number
	Size: number
	TimeScale: number
	function FastForward(self, numFrames: number): nil
end

declare class SmoothVoxelsUpgraderService extends Instance
	Status: RBXScriptSignal<number>
	function Cancel(self): nil
	function Start(self): nil
end

declare class SnippetService extends Instance
end

declare class SocialService extends Instance
	GameInvitePromptClosed: RBXScriptSignal<Instance, { any }>
	IrisInvitePromptClosed: RBXScriptSignal<Instance>
	OnIrisInviteInvoked: (tag: string, irisParticipantIds: { any }) -> Instance
	PromptInviteRequested: RBXScriptSignal<Instance, Instance>
	PromptIrisInviteRequested: RBXScriptSignal<Instance, string>
	SelfViewHidden: RBXScriptSignal<>
	SelfViewVisible: RBXScriptSignal<EnumSelfViewPosition>
	function CanSendGameInviteAsync(self, player: Player, recipientId: number?): boolean
	function CanSendIrisInviteAsync(self, player: Instance): boolean
	function HideSelfView(self): nil
	function InvokeGameInvitePromptClosed(self, player: Instance, recipientIds: { any }): nil
	function InvokeIrisInvite(self, player: Instance, tag: string, irisParticipantIds: { any }): nil
	function InvokeIrisInvitePromptClosed(self, player: Instance): nil
	function PromptGameInvite(self, player: Player, experienceInviteOptions: Instance?): nil
	function PromptIrisInvite(self, player: Instance, tag: string): nil
	function ShowSelfView(self, selfViewPosition: EnumSelfViewPosition?): nil
end

declare class Sound extends Instance
	ChannelCount: number
	DidLoop: RBXScriptSignal<string, number>
	Ended: RBXScriptSignal<string>
	IsLoaded: boolean
	IsPaused: boolean
	IsPlaying: boolean
	IsSpatial: boolean
	Loaded: RBXScriptSignal<string>
	LoopRegion: NumberRange
	Looped: boolean
	Paused: RBXScriptSignal<string>
	PlayOnRemove: boolean
	PlaybackLoudness: number
	PlaybackRegion: NumberRange
	PlaybackRegionsEnabled: boolean
	PlaybackSpeed: number
	Played: RBXScriptSignal<string>
	Playing: boolean
	Resumed: RBXScriptSignal<string>
	RollOffGain: number
	RollOffMaxDistance: number
	RollOffMinDistance: number
	RollOffMode: EnumRollOffMode
	SoundGroup: SoundGroup
	SoundId: Content
	Stopped: RBXScriptSignal<string>
	TimeLength: number
	TimePosition: number
	UsageContextPermission: EnumUsageContext
	Volume: number
	function Pause(self): nil
	function Play(self): nil
	function Resume(self): nil
	function Stop(self): nil
end

declare class SoundEffect extends Instance
	Enabled: boolean
	Priority: number
end

declare class ChorusSoundEffect extends SoundEffect
	Depth: number
	Mix: number
	Rate: number
end

declare class CompressorSoundEffect extends SoundEffect
	Attack: number
	GainMakeup: number
	Ratio: number
	Release: number
	SideChain: Instance
	Threshold: number
end

declare class CustomSoundEffect extends SoundEffect
end

declare class AssetSoundEffect extends CustomSoundEffect
end

declare class ChannelSelectorSoundEffect extends CustomSoundEffect
	Channel: number
end

declare class DistortionSoundEffect extends SoundEffect
	Level: number
end

declare class EchoSoundEffect extends SoundEffect
	Delay: number
	DryLevel: number
	Feedback: number
	WetLevel: number
end

declare class EqualizerSoundEffect extends SoundEffect
	HighGain: number
	LowGain: number
	MidGain: number
end

declare class FlangeSoundEffect extends SoundEffect
	Depth: number
	Mix: number
	Rate: number
end

declare class PitchShiftSoundEffect extends SoundEffect
	Octave: number
end

declare class ReverbSoundEffect extends SoundEffect
	DecayTime: number
	Density: number
	Diffusion: number
	DryLevel: number
	WetLevel: number
end

declare class TremoloSoundEffect extends SoundEffect
	Depth: number
	Duty: number
	Frequency: number
end

declare class SoundGroup extends Instance
	Volume: number
end

declare class SoundService extends Instance
	AmbientReverb: EnumReverbType
	DeviceListChanged: RBXScriptSignal<any>
	DistanceFactor: number
	DopplerScale: number
	RespectFilteringEnabled: boolean
	RolloffScale: number
	VolumetricAudio: EnumVolumetricAudio
	function BeginRecording(self): boolean
	function EndRecording(self): { [any]: any }
	function GetInputDevice(self): any
	function GetInputDevices(self): any
	function GetListener(self): (EnumListenerType, any)
	function GetOutputDevice(self): any
	function GetOutputDevices(self): any
	function GetRecordingDevices(self): { [any]: any }
	function GetSoundMemoryData(self): { [any]: any }
	function PlayLocalSound(self, sound: Sound): nil
	function SetInputDevice(self, name: string, guid: string): nil
	function SetListener(self, listenerType: EnumListenerType, listener: any): nil
	function SetOutputDevice(self, name: string, guid: string): nil
	function SetRecordingDevice(self, deviceIndex: number): boolean
end

declare class Sparkles extends Instance
	Color: Color3
	Enabled: boolean
	SparkleColor: Color3
	TimeScale: number
	function FastForward(self, numFrames: number): nil
end

declare class SpawnerService extends Instance
end

declare class StackFrame extends Instance
	FrameId: number
	FrameName: string
	FrameType: EnumDebuggerFrameType
	Globals: DebuggerVariable
	Line: number
	Locals: DebuggerVariable
	Populated: boolean
	Script: string
	Upvalues: DebuggerVariable
end

declare class StandalonePluginScripts extends Instance
end

declare class StarterGear extends Instance
end

declare class StarterPack extends Instance
end

declare class StarterPlayer extends Instance
	AllowCustomAnimations: boolean
	AutoJumpEnabled: boolean
	AvatarJointUpgrade: EnumAvatarJointUpgrade
	CameraMaxZoomDistance: number
	CameraMinZoomDistance: number
	CameraMode: EnumCameraMode
	CharacterJumpHeight: number
	CharacterJumpPower: number
	CharacterMaxSlopeAngle: number
	CharacterUseJumpPower: boolean
	CharacterWalkSpeed: number
	DeathStyle: EnumDeathStyle
	DevCameraOcclusionMode: EnumDevCameraOcclusionMode
	DevComputerCameraMovementMode: EnumDevComputerCameraMovementMode
	DevComputerMovementMode: EnumDevComputerMovementMode
	DevTouchCameraMovementMode: EnumDevTouchCameraMovementMode
	DevTouchMovementMode: EnumDevTouchMovementMode
	EnableDynamicHeads: EnumLoadDynamicHeads
	EnableMouseLockOption: boolean
	GameSettingsAssetIDFace: number
	GameSettingsAssetIDHead: number
	GameSettingsAssetIDLeftArm: number
	GameSettingsAssetIDLeftLeg: number
	GameSettingsAssetIDPants: number
	GameSettingsAssetIDRightArm: number
	GameSettingsAssetIDRightLeg: number
	GameSettingsAssetIDShirt: number
	GameSettingsAssetIDTeeShirt: number
	GameSettingsAssetIDTorso: number
	GameSettingsAvatar: EnumGameAvatarType
	GameSettingsR15Collision: EnumR15CollisionType
	GameSettingsScaleRangeBodyType: NumberRange
	GameSettingsScaleRangeHead: NumberRange
	GameSettingsScaleRangeHeight: NumberRange
	GameSettingsScaleRangeProportion: NumberRange
	GameSettingsScaleRangeWidth: NumberRange
	HealthDisplayDistance: number
	HumanoidStateMachineMode: EnumHumanoidStateMachineMode
	LoadCharacterAppearance: boolean
	NameDisplayDistance: number
	UserEmotesEnabled: boolean
	function ClearDefaults(self): nil
end

declare class StarterPlayerScripts extends Instance
end

declare class StarterCharacterScripts extends StarterPlayerScripts
end

declare class Stats extends Instance
	ContactsCount: number
	DataReceiveKbps: number
	DataSendKbps: number
	HeartbeatTimeMs: number
	InstanceCount: number
	MovingPrimitivesCount: number
	PhysicsReceiveKbps: number
	PhysicsSendKbps: number
	PhysicsStepTimeMs: number
	PrimitivesCount: number
	function GetBrowserTrackerId(self): string
	function GetMemoryUsageMbForTag(self, tag: EnumDeveloperMemoryTag): number
	function GetPaginatedMemoryByTexture(self, queryType: EnumTextureQueryType, pageIndex: number, pageSize: number): { [any]: any }
	function GetTotalMemoryUsageMb(self): number
end

declare class StatsItem extends Instance
	DisplayName: string
	function GetValue(self): number
	function GetValueString(self): string
end

declare class RunningAverageItemDouble extends StatsItem
end

declare class RunningAverageItemInt extends StatsItem
end

declare class RunningAverageTimeIntervalItem extends StatsItem
end

declare class TotalCountTimeIntervalItem extends StatsItem
end

declare class StopWatchReporter extends Instance
	function FinishTask(self, taskId: number): nil
	function SendReport(self, reportName: string): nil
	function StartTask(self, reportName: string, taskName: string): number
end

declare class Studio extends Instance
	CommandBarLocalState: boolean
	DefaultScriptFileDir: QDir
	DeprecatedObjectsShown: boolean
	DisplayLanguage: string
	EnableIndentationRulers: boolean
	EnableOnTypeAutocomplete: boolean
	Font: QFont
	HintColor: Color3
	IconOverrideDir: QDir
	IndentationRulerColor: Color3
	InformationColor: Color3
	LocalAssetsFolder: QDir
	LuaDebuggerEnabled: boolean
	LuaDebuggerEnabledAtStartup: boolean
	PermissionLevelShown: EnumPermissionLevelShown
	PluginDebuggingEnabled: boolean
	PluginsDir: QDir
	ReloadBuiltinPluginsOnChange: boolean
	ReloadLocalPluginsOnChange: boolean
	Rulers: string
	RuntimeUndoBehavior: EnumRuntimeUndoBehavior
	ScriptEditorMenuBorderColor: Color3
	ScriptEditorShouldShowPluginMethods: boolean
	ScriptTimeoutLength: number
	ShowCorePackagesInExplorer: boolean
	Theme: StudioTheme
	ThemeChanged: RBXScriptSignal<>
	function GetAvailableThemes(self): { any }
end

declare class StudioAssetService extends Instance
	OnConvertToPackageResult: RBXScriptSignal<boolean, string>
	OnPublishPackageResult: RBXScriptSignal<{ [any]: any }, string>
	OnSaveToRoblox: RBXScriptSignal<{ Instance }, any>
	function ConvertToPackageUpload(self, uploadUrl: string, cloneInstances: { Instance }, originalInstances: { Instance }): nil
	function PublishPackage(self, instance: Instance, publishInfo: { [any]: any }): nil
	function SerializeInstances(self, instances: { Instance }): string
	function ShowSaveToRoblox(self, instances: { Instance }, assetType: any): nil
end

declare class StudioCallout extends Instance
	AnchorPoint: Vector2
	IsArrowVisible: boolean
	IsNextVisible: boolean
	RowName: string
	Text: string
	Title: string
	function SetOnNextClicked(self, onClick: ((...any) -> ...any)): nil
end

declare class StudioData extends Instance
	EnableScriptCollabByDefaultOnLoad: boolean
end

declare class StudioDeviceEmulatorService extends Instance
	CurrentDeviceIdChanged: RBXScriptSignal<>
	HasMultiTouchStarted: boolean
	IsMultiTouchEmulationOn: boolean
	IsMultiTouchEnabled: boolean
	OrientationChanged: RBXScriptSignal<>
	PivotPosition: Vector2
	TouchInBoundsChanged: RBXScriptSignal<>
	TouchPositionsChanged: RBXScriptSignal<>
	function EmulatePCDeviceWithResolution(self, deviceId: string, resolution: Vector2): boolean
	function GetCurrentDeviceId(self): string
	function GetCurrentOrientation(self): EnumScreenOrientation
	function GetMaxNumTouches(self): number
	function GetTouchInBounds(self, index: number): boolean
	function GetTouchPosition(self, index: number): Vector2
	function HasDeviceWithId(self, deviceId: string): boolean
	function SetCurrentDeviceId(self, deviceId: string): nil
	function SetCurrentOrientation(self, orientation: EnumScreenOrientation): nil
end

declare class StudioObjectBase extends Instance
end

declare class StudioWidget extends StudioObjectBase
end

declare class StudioPublishService extends Instance
	GameNameUpdated: RBXScriptSignal<string>
	GamePublishCancelled: RBXScriptSignal<>
	GamePublishFinished: RBXScriptSignal<boolean, number, string>
	OnSaveOrPublishPlaceToRoblox: RBXScriptSignal<boolean, boolean, EnumStudioCloseMode>
	function ClearUploadNames(self): nil
	function PublishAs(self, universeId: number, placeId: number, groupId: number, isPublish: boolean, publishParameters: any): nil
	function PublishThenTurnOnTeamCreate(self): nil
	function RefreshDocumentDisplayName(self): nil
	function SetTeamCreateOnPublishInfo(self, shouldTurnOnTcOnPublish: boolean, newPlaceName: string): nil
	function SetUniverseDisplayName(self, newName: string): nil
	function SetUploadNames(self, placeName: string, universeName: string): nil
	function ShowSaveOrPublishPlaceToRoblox(self, showGameSelect: boolean, isPublish: boolean, closeMode: EnumStudioCloseMode): nil
end

declare class StudioScriptDebugEventListener extends Instance
end

declare class StudioSdkService extends Instance
	function GetSdk(self): Instance
	function SetSdk(self, sdk: Instance): nil
end

declare class StudioService extends Instance
	ActiveScript: Instance
	AlignDraggedObjects: boolean
	DraggerSolveConstraints: boolean
	GridSize: number
	HoverInstance: Instance
	InstalledPluginData: string
	OnImportFromRoblox: RBXScriptSignal<>
	OnOpenGameSettings: RBXScriptSignal<string>
	OnOpenManagePackagePlugin: RBXScriptSignal<number, number>
	OnPluginInstalledFromToolbox: RBXScriptSignal<>
	OnPluginInstalledFromWeb: RBXScriptSignal<string>
	OnPublishAsPlugin: RBXScriptSignal<{ Instance }>
	OnSaveToRoblox: RBXScriptSignal<{ Instance }>
	PivotSnapToGeometry: boolean
	PromptTransformPluginCheckEnable: RBXScriptSignal<>
	RotateIncrement: number
	SaveLocallyAsComplete: RBXScriptSignal<boolean>
	ShowConstraintDetails: boolean
	StudioLocaleId: string
	UseLocalSpace: boolean
	function AnimationIdSelected(self, id: number): nil
	function CopyToClipboard(self, stringToCopy: string): nil
	function GetBadgeConfigureUrl(self, badgeId: number): string
	function GetBadgeUploadUrl(self): string
	function GetClassIcon(self, className: string): { [any]: any }
	function GetPlaceIsPersistedToCloud(self): boolean
	function GetResourceByCategory(self, category: string): { [any]: any }
	function GetStartupAssetId(self): string
	function GetStartupPluginId(self): string
	function GetTermsOfUseUrl(self): string
	function GetUserId(self): number
	function GizmoRaycast(self, origin: Vector3, direction: Vector3, raycastParams: RaycastParams?): RaycastResult
	function HasInternalPermission(self): boolean
	function IsPluginInstalled(self, assetId: number): boolean
	function IsPluginUpToDate(self, assetId: number, currentAssetVersion: number): boolean
	function OpenInBrowser_DONOTUSE(self, url: string): nil
	function PromptImportFile(self, fileTypeFilter: { any }?): Instance
	function PromptImportFiles(self, fileTypeFilter: { any }?): { Instance }
	function RequestClose(self, closeMode: EnumStudioCloseMode): nil
	function SetPluginEnabled(self, assetId: number, state: boolean): nil
	function ShowPlaceVersionHistoryDialog(self, placeId: number): nil
	function ShowPublishToRoblox(self): nil
	function TryInstallPlugin(self, assetId: number, assetVersionId: number): nil
	function UninstallPlugin(self, assetId: number): nil
	function UpdatePluginManagement(self): nil
end

declare class StudioTheme extends Instance
	function GetColor(self, styleguideitem: EnumStudioStyleGuideColor, modifier: EnumStudioStyleGuideModifier?): Color3
end

declare class StyleBase extends Instance
	StyleRulesChanged: RBXScriptSignal<>
	function GetStyleRules(self): { Instance }
	function InsertStyleRule(self, rule: StyleRule, index: number?): nil
	function SetStyleRules(self, rules: { Instance }): nil
end

declare class StyleRule extends StyleBase
	Selector: string
	SelectorError: string
	function GetProperties(self): { [any]: any }
	function GetPropertiesResolved(self): { [any]: any }
	function GetProperty(self, name: string): any
	function GetPropertyResolved(self, name: string): any
	function SetProperties(self, table: { [any]: any }): nil
	function SetProperty(self, name: string, value: any): nil
end

declare class StyleSheet extends StyleBase
	function GetDerives(self): { Instance }
	function SetDerives(self, derives: { Instance }): nil
end

declare class StyleDerive extends Instance
	StyleSheet: StyleSheet
end

declare class StyleLink extends Instance
	StyleSheet: StyleSheet
end

declare class StylingService extends Instance
	function GetAppliedStyles(self, instance: Instance): { any }
	function GetStyleInfo(self, style: StyleRule): { [any]: any }
	function GetStyleSheetDerivesChain(self, styleSheet: StyleSheet): { Instance }
	function GetStyleSheetInfo(self, styleSheet: StyleSheet): { [any]: any }
end

declare class SurfaceAppearance extends Instance
	AlphaMode: EnumAlphaMode
	ColorMap: Content
	MetalnessMap: Content
	NormalMap: Content
	RoughnessMap: Content
	TexturePack: Content
end

declare class TaskScheduler extends Instance
	SchedulerDutyCycle: number
	SchedulerRate: number
	ThreadPoolConfig: EnumThreadPoolConfig
	ThreadPoolSize: number
end

declare class Team extends Instance
	AutoAssignable: boolean
	ChildOrder: number
	PlayerAdded: RBXScriptSignal<Player>
	PlayerRemoved: RBXScriptSignal<Player>
	TeamColor: BrickColor
	function GetPlayers(self): { Player }
end

declare class TeamCreateData extends Instance
end

declare class TeamCreatePublishService extends Instance
end

declare class TeamCreateService extends Instance
	ToggleManageCollaborators: RBXScriptSignal<>
	function CloseGameIfUserDoesntHavePerms(self): nil
end

declare class Teams extends Instance
	function GetTeams(self): { Team }
end

declare class TeleportAsyncResult extends Instance
	PrivateServerId: string
	ReservedServerAccessCode: string
end

declare class TeleportOptions extends Instance
	ReservedServerAccessCode: string
	ServerInstanceId: string
	ShouldReserveServer: boolean
	function GetTeleportData(self): any
	function SetTeleportData(self, teleportData: any): nil
end

declare class TeleportService extends Instance
	LocalPlayerArrivedFromTeleport: RBXScriptSignal<Player, any>
	MenuTeleportAttempt: RBXScriptSignal<>
	TeleportInitFailed: RBXScriptSignal<Player, EnumTeleportResult, string, number, TeleportOptions>
	function Block(self): nil
	function GetArrivingTeleportGui(self): ScreenGui
	function GetLocalPlayerTeleportData(self): any
	function GetPlayerPlaceInstanceAsync(self, userId: number): (boolean, string, number, string)
	function GetTeleportSetting(self, setting: string): any
	function ReserveServer(self, placeId: number): (string, string)
	function SetTeleportGui(self, gui: GuiObject): nil
	function SetTeleportSetting(self, setting: string, value: any): nil
	function Teleport(self, placeId: number, player: Player?, teleportData: any, customLoadingScreen: GuiObject?): nil
	function TeleportAsync(self, placeId: number, players: { Player }, teleportOptions: TeleportOptions?): TeleportAsyncResult
	function TeleportCancel(self): nil
	function TeleportPartyAsync(self, placeId: number, players: { Player }, teleportData: any, customLoadingScreen: GuiObject?): string
	function TeleportToPlaceInstance(self, placeId: number, instanceId: string, player: Player?, spawnName: string?, teleportData: any, customLoadingScreen: GuiObject?): nil
	function TeleportToPrivateServer(self, placeId: number, reservedServerAccessCode: string, players: { Player }, spawnName: string?, teleportData: any, customLoadingScreen: GuiObject?): nil
	function TeleportToSpawnByName(self, placeId: number, spawnName: string, player: Player?, teleportData: any, customLoadingScreen: GuiObject?): nil
	function UnblockAsync(self): any
end

declare class TemporaryCageMeshProvider extends Instance
end

declare class TemporaryScriptService extends Instance
end

declare class TerrainDetail extends Instance
	ColorMap: Content
	Face: EnumTerrainFace
	MaterialPattern: EnumMaterialPattern
	MetalnessMap: Content
	NormalMap: Content
	RoughnessMap: Content
	StudsPerTile: number
end

declare class TerrainRegion extends Instance
	SizeInCells: Vector3
	function ApplyTransform(self, rotation: CFrame, size: Vector3): nil
	function ApplyTransformSubregion(self, rotation: CFrame, size: Vector3, region: Region3int16): TerrainRegion
	function GetRegionWireframe(self): { any }
end

declare class TestService extends Instance
	AutoRuns: boolean
	Description: string
	ErrorCount: number
	ExecuteWithStudioRun: boolean
	Is30FpsThrottleEnabled: boolean
	IsPhysicsEnvironmentalThrottled: boolean
	IsSleepAllowed: boolean
	NumberOfPlayers: number
	ServerCollectConditionalResult: RBXScriptSignal<boolean, string, Instance, number>
	ServerCollectResult: RBXScriptSignal<string, Instance, number>
	SimulateSecondsLag: number
	TestCount: number
	Timeout: number
	WarnCount: number
	function Check(self, condition: boolean, description: string, source: Instance?, line: number?): nil
	function Checkpoint(self, text: string, source: Instance?, line: number?): nil
	function Done(self): nil
	function Error(self, description: string, source: Instance?, line: number?): nil
	function Fail(self, description: string, source: Instance?, line: number?): nil
	function Message(self, text: string, source: Instance?, line: number?): nil
	function Require(self, condition: boolean, description: string, source: Instance?, line: number?): nil
	function Run(self): nil
	function ScopeTime(self): { [any]: any }
	function Warn(self, condition: boolean, description: string, source: Instance?, line: number?): nil
	function isFeatureEnabled(self, name: string): boolean
end

declare class TextBoxService extends Instance
end

declare class TextChannel extends Instance
	MessageReceived: RBXScriptSignal<TextChatMessage>
	OnIncomingMessage: (message: TextChatMessage) -> any
	ShouldDeliverCallback: (message: TextChatMessage, textSource: TextSource) -> any
	function AddUserAsync(self, userId: number): any
	function DisplaySystemMessage(self, systemMessage: string, metadata: string?): TextChatMessage
	function SendAsync(self, message: string, metadata: string?): TextChatMessage
end

declare class TextChatCommand extends Instance
	Enabled: boolean
	PrimaryAlias: string
	SecondaryAlias: string
	Triggered: RBXScriptSignal<TextSource, string>
end

declare class TextChatConfigurations extends Instance
end

declare class BubbleChatConfiguration extends TextChatConfigurations
	AdorneeName: string
	BackgroundColor3: Color3
	BackgroundTransparency: number
	BubbleDuration: number
	BubblesSpacing: number
	Enabled: boolean
	Font: EnumFont
	FontFace: Font
	LocalPlayerStudsOffset: Vector3
	MaxBubbles: number
	MaxDistance: number
	MinimizeDistance: number
	TailVisible: boolean
	TextColor3: Color3
	TextSize: number
	VerticalStudsOffset: number
end

declare class ChatInputBarConfiguration extends TextChatConfigurations
	AbsolutePosition: Vector2
	AbsolutePositionWrite: Vector2
	AbsoluteSize: Vector2
	AbsoluteSizeWrite: Vector2
	BackgroundColor3: Color3
	BackgroundTransparency: number
	Enabled: boolean
	FontFace: Font
	IsFocused: boolean
	IsFocusedWrite: boolean
	KeyboardKeyCode: EnumKeyCode
	PlaceholderColor3: Color3
	TargetTextChannel: TextChannel
	TextBox: TextBox
	TextColor3: Color3
	TextSize: number
	TextStrokeColor3: Color3
	TextStrokeTransparency: number
end

declare class ChatWindowConfiguration extends TextChatConfigurations
	AbsolutePosition: Vector2
	AbsolutePositionWrite: Vector2
	AbsoluteSize: Vector2
	AbsoluteSizeWrite: Vector2
	BackgroundColor3: Color3
	BackgroundTransparency: number
	Enabled: boolean
	FontFace: Font
	HeightScale: number
	HorizontalAlignment: EnumHorizontalAlignment
	TextColor3: Color3
	TextSize: number
	TextStrokeColor3: Color3
	TextStrokeTransparency: number
	VerticalAlignment: EnumVerticalAlignment
	WidthScale: number
end

declare class TextChatMessage extends Instance
	BubbleChatMessageProperties: BubbleChatMessageProperties
	MessageId: string
	Metadata: string
	PrefixText: string
	Status: EnumTextChatMessageStatus
	Text: string
	TextChannel: TextChannel
	TextSource: TextSource
	Timestamp: DateTime
end

declare class TextChatMessageProperties extends Instance
	PrefixText: string
	Text: string
end

declare class TextChatService extends Instance
	BubbleDisplayed: RBXScriptSignal<Instance, TextChatMessage>
	ChatVersion: EnumChatVersion
	CreateDefaultCommands: boolean
	CreateDefaultTextChannels: boolean
	MessageReceived: RBXScriptSignal<TextChatMessage>
	OnBubbleAdded: (message: TextChatMessage, adornee: Instance) -> any
	OnIncomingMessage: (message: TextChatMessage) -> any
	SendingMessage: RBXScriptSignal<TextChatMessage>
	function CanUserChatAsync(self, userId: number): boolean
	function CanUsersChatAsync(self, userIdFrom: number, userIdTo: number): boolean
	function DisplayBubble(self, partOrCharacter: Instance, message: string): nil
end

declare class TextFilterResult extends Instance
	function GetChatForUserAsync(self, toUserId: number): string
	function GetNonChatStringForBroadcastAsync(self): string
	function GetNonChatStringForUserAsync(self, toUserId: number): string
end

declare class TextFilterTranslatedResult extends Instance
	SourceLanguage: string
	SourceText: TextFilterResult
	function GetTranslationForLocale(self, locale: string): TextFilterResult
	function GetTranslations(self): { [any]: any }
end

declare class TextService extends Instance
	function FilterAndTranslateStringAsync(self, stringToFilter: string, fromUserId: number, targetLocales: { any }, textContext: EnumTextFilterContext?): Instance
	function FilterStringAsync(self, stringToFilter: string, fromUserId: number, textContext: EnumTextFilterContext?): Instance
	function GetFamilyInfoAsync(self, assetId: Content): { [any]: any }
	function GetFontMemoryData(self): { [any]: any }
	function GetTextBoundsAsync(self, params: GetTextBoundsParams): Vector2
	function GetTextSize(self, string: string, fontSize: number, font: EnumFont, frameSize: Vector2): Vector2
	function SetResolutionScale(self, scale: number): nil
end

declare class TextSource extends Instance
	CanSend: boolean
	UserId: number
end

declare class ThirdPartyUserService extends Instance
	ActiveUserSignedOut: RBXScriptSignal<number>
	function GetUserPlatformId(self): string
	function GetUserPlatformName(self): string
	function HaveActiveUser(self): boolean
	function IsOver13(self): boolean
	function RegisterActiveUser(self, gamepadId: EnumUserInputType): number
	function ReturnToEngagement(self): nil
	function ShowAccountPicker(self): nil
end

declare class ThreadState extends Instance
	FrameCount: number
	Populated: boolean
	ThreadId: number
	ThreadName: string
	function GetFrame(self, index: number): Instance
end

declare class TimerService extends Instance
end

declare class ToastNotificationService extends Instance
	function HideNotification(self, notificationId: string): nil
	function ShowNotification(self, message: string, notificationId: string): nil
end

declare class TouchInputService extends Instance
end

declare class TouchTransmitter extends Instance
end

declare class TracerService extends Instance
	function FinishSpan(self, spanId: string): nil
	function StartSpan(self, name: string, parentId: string): string
end

declare class TrackerLodController extends Instance
	AudioMode: EnumTrackerLodFlagMode
	UpdateState: RBXScriptSignal<>
	VideoExtrapolationMode: EnumTrackerExtrapolationFlagMode
	VideoLodMode: EnumTrackerLodValueMode
	VideoMode: EnumTrackerLodFlagMode
	function getExtrapolation(self): number
	function getVideoLod(self): number
	function isAudioEnabled(self): boolean
	function isVideoEnabled(self): boolean
end

declare class TrackerStreamAnimation extends Instance
end

declare class Trail extends Instance
	Attachment0: Attachment
	Attachment1: Attachment
	Brightness: number
	Color: ColorSequence
	Enabled: boolean
	FaceCamera: boolean
	Lifetime: number
	LightEmission: number
	LightInfluence: number
	MaxLength: number
	MinLength: number
	Texture: Content
	TextureLength: number
	TextureMode: EnumTextureMode
	Transparency: NumberSequence
	WidthScale: NumberSequence
	function Clear(self): nil
end

declare class Translator extends Instance
	LocaleId: string
	function FormatByKey(self, key: string, args: any): string
	function RobloxOnlyTranslate(self, context: Instance, text: string): string
	function Translate(self, context: Instance, text: string): string
end

declare class TutorialService extends Instance
	function GetMainViewSessionId(self): string
	function GetStudioWidgetFromLabel(self, label: string): StudioWidget
	function HasUserCompletedTutorial(self): boolean
	function PromptClosePlace(self): nil
	function SetTutorialCompletionStatus(self, completed: boolean): nil
	function ShouldLaunchTutorial(self): boolean
end

declare class TweenBase extends Instance
	Completed: RBXScriptSignal<EnumPlaybackState>
	PlaybackState: EnumPlaybackState
	function Cancel(self): nil
	function Pause(self): nil
	function Play(self): nil
end

declare class Tween extends TweenBase
	Instance: Instance
	TweenInfo: TweenInfo
end

declare class TweenService extends Instance
	function Create(self, instance: Instance, tweenInfo: TweenInfo, propertyTable: { [any]: any }): Tween
	function GetValue(self, alpha: number, easingStyle: EnumEasingStyle, easingDirection: EnumEasingDirection): number
end

declare class UGCAvatarService extends Instance
end

declare class UGCValidationService extends Instance
	function CalculateUniqueUVCount(self, meshId: string): number
	function CanLoadAsset(self, assetId: string): boolean
	function FetchAssetWithFormat(self, url: Content, assetFormat: string): { Instance }
	function GetMeshTriCount(self, meshId: string): number
	function GetMeshVertColors(self, meshId: string): { any }
	function GetMeshVerts(self, meshId: string): { any }
	function GetPropertyValue(self, instance: Instance, property: string): any
	function GetTextureSize(self, textureId: string): Vector2
	function ResetCollisionFidelity(self, meshPart: Instance): nil
	function SetMeshIdBlocking(self, meshPart: Instance, meshId: string): nil
	function ValidateCageMeshIntersection(self, innerCageMeshId: string, outerCageMeshId: string, refMeshId: string): any
	function ValidateCageNonManifoldAndHoles(self, meshId: string): any
	function ValidateDynamicHeadMesh(self, meshId: string): boolean
	function ValidateFullBodyCageDeletion(self, meshId: string): boolean
	function ValidateMeshBounds(self, meshId: string, meshScale: Vector3, boundsOffset: Vector3, attachmentCF: CFrame, handleCF: CFrame): boolean
	function ValidateMeshTriangles(self, meshId: string): boolean
	function ValidateMeshVertColors(self, meshId: string, includeAlpha: boolean?): boolean
	function ValidateMisMatchUV(self, innerCageMeshId: string, outerCageMeshId: string): boolean
	function ValidateNumTextureChannels(self, textureId: string, numChannelsRequired: number): boolean
	function ValidateOverlappingVertices(self, meshId: string): boolean
	function ValidateSkinnedMesh(self, meshId: string): boolean
	function ValidateTextureSize(self, textureId: string): boolean
	function ValidateUVSpace(self, meshId: string): boolean
	function ValidateUniqueUVCount(self, meshId: string, numRequired: number): boolean
end

declare class UIBase extends Instance
end

declare class UIComponent extends UIBase
end

declare class UIConstraint extends UIComponent
end

declare class UIAspectRatioConstraint extends UIConstraint
	AspectRatio: number
	AspectType: EnumAspectType
	DominantAxis: EnumDominantAxis
end

declare class UISizeConstraint extends UIConstraint
	MaxSize: Vector2
	MinSize: Vector2
end

declare class UITextSizeConstraint extends UIConstraint
	MaxTextSize: number
	MinTextSize: number
end

declare class UICorner extends UIComponent
	CornerRadius: UDim
end

declare class UIGradient extends UIComponent
	Color: ColorSequence
	Enabled: boolean
	Offset: Vector2
	Rotation: number
	Transparency: NumberSequence
end

declare class UILayout extends UIComponent
end

declare class UIGridStyleLayout extends UILayout
	AbsoluteContentSize: Vector2
	FillDirection: EnumFillDirection
	HorizontalAlignment: EnumHorizontalAlignment
	SortOrder: EnumSortOrder
	VerticalAlignment: EnumVerticalAlignment
end

declare class UIGridLayout extends UIGridStyleLayout
	AbsoluteCellCount: Vector2
	AbsoluteCellSize: Vector2
	CellPadding: UDim2
	CellSize: UDim2
	FillDirectionMaxCells: number
	StartCorner: EnumStartCorner
end

declare class UIListLayout extends UIGridStyleLayout
	Padding: UDim
end

declare class UIPageLayout extends UIGridStyleLayout
	Animated: boolean
	Circular: boolean
	CurrentPage: GuiObject
	EasingDirection: EnumEasingDirection
	EasingStyle: EnumEasingStyle
	GamepadInputEnabled: boolean
	Padding: UDim
	PageEnter: RBXScriptSignal<Instance>
	PageLeave: RBXScriptSignal<Instance>
	ScrollWheelInputEnabled: boolean
	Stopped: RBXScriptSignal<Instance>
	TouchInputEnabled: boolean
	TweenTime: number
	function JumpTo(self, page: Instance): nil
	function JumpToIndex(self, index: number): nil
	function Next(self): nil
	function Previous(self): nil
end

declare class UITableLayout extends UIGridStyleLayout
	FillEmptySpaceColumns: boolean
	FillEmptySpaceRows: boolean
	MajorAxis: EnumTableMajorAxis
	Padding: UDim2
end

declare class UIPadding extends UIComponent
	PaddingBottom: UDim
	PaddingLeft: UDim
	PaddingRight: UDim
	PaddingTop: UDim
end

declare class UIScale extends UIComponent
	Scale: number
end

declare class UIStroke extends UIComponent
	ApplyStrokeMode: EnumApplyStrokeMode
	Color: Color3
	Enabled: boolean
	LineJoinMode: EnumLineJoinMode
	Thickness: number
	Transparency: number
end

declare class UnvalidatedAssetService extends Instance
	function AppendTempAssetId(self, userId: number, id: number, lookAt: Vector3, camPos: Vector3, usage: string): nil
	function AppendVantagePoint(self, userId: number, id: number, lookAt: Vector3, camPos: Vector3): boolean
	function UpgradeTempAssetId(self, userId: number, tempId: number, assetId: number): boolean
end

declare class UserGameSettings extends Instance
	AllTutorialsDisabled: boolean
	CameraMode: EnumCustomCameraMode
	CameraYInverted: boolean
	ChatVisible: boolean
	ComputerCameraMovementMode: EnumComputerCameraMovementMode
	ComputerMovementMode: EnumComputerMovementMode
	ControlMode: EnumControlMode
	DefaultCameraID: string
	Fullscreen: boolean
	FullscreenChanged: RBXScriptSignal<boolean>
	GamepadCameraSensitivity: number
	GraphicsQualityLevel: number
	HasEverUsedVR: boolean
	IsUsingCameraYInverted: boolean
	IsUsingGamepadCameraSensitivity: boolean
	MasterVolume: number
	MicroProfilerWebServerEnabled: boolean
	MicroProfilerWebServerIP: string
	MicroProfilerWebServerPort: number
	MouseSensitivity: number
	MouseSensitivityFirstPerson: Vector2
	MouseSensitivityThirdPerson: Vector2
	OnScreenProfilerEnabled: boolean
	OnboardingsCompleted: string
	PerformanceStatsVisible: boolean
	PerformanceStatsVisibleChanged: RBXScriptSignal<boolean>
	PlayerHeight: number
	PreferredTransparency: number
	RCCProfilerRecordFrameRate: number
	RCCProfilerRecordTimeFrame: number
	ReducedMotion: boolean
	RotationType: EnumRotationType
	SavedQualityLevel: EnumSavedQualitySetting
	StartMaximized: boolean
	StartScreenPosition: Vector2
	StartScreenSize: Vector2
	StudioModeChanged: RBXScriptSignal<boolean>
	TouchCameraMovementMode: EnumTouchCameraMovementMode
	TouchMovementMode: EnumTouchMovementMode
	UiNavigationKeyBindEnabled: boolean
	UsedCoreGuiIsVisibleToggle: boolean
	UsedCustomGuiIsVisibleToggle: boolean
	UsedHideHudShortcut: boolean
	VREnabled: boolean
	VRRotationIntensity: number
	VRSafetyBubbleMode: EnumVRSafetyBubbleMode
	VRSmoothRotationEnabled: boolean
	VignetteEnabled: boolean
	function GetCameraYInvertValue(self): number
	function GetOnboardingCompleted(self, onboardingId: string): boolean
	function GetTutorialState(self, tutorialId: string): boolean
	function InFullScreen(self): boolean
	function InStudioMode(self): boolean
	function ResetOnboardingCompleted(self, onboardingId: string): nil
	function SetCameraYInvertVisible(self): nil
	function SetGamepadCameraSensitivityVisible(self): nil
	function SetOnboardingCompleted(self, onboardingId: string): nil
	function SetTutorialState(self, tutorialId: string, value: boolean): nil
end

declare class UserInputService extends Instance
	AccelerometerEnabled: boolean
	BottomBarSize: Vector2
	DeviceAccelerationChanged: RBXScriptSignal<InputObject>
	DeviceGravityChanged: RBXScriptSignal<InputObject>
	DeviceRotationChanged: RBXScriptSignal<InputObject, CFrame>
	GamepadConnected: RBXScriptSignal<EnumUserInputType>
	GamepadDisconnected: RBXScriptSignal<EnumUserInputType>
	GamepadEnabled: boolean
	GyroscopeEnabled: boolean
	InputBegan: RBXScriptSignal<InputObject, boolean>
	InputChanged: RBXScriptSignal<InputObject, boolean>
	InputEnded: RBXScriptSignal<InputObject, boolean>
	JumpRequest: RBXScriptSignal<>
	KeyboardEnabled: boolean
	LastInputTypeChanged: RBXScriptSignal<EnumUserInputType>
	LegacyInputEventsEnabled: boolean
	MouseBehavior: EnumMouseBehavior
	MouseDeltaSensitivity: number
	MouseEnabled: boolean
	MouseIcon: Content
	MouseIconEnabled: boolean
	NavBarSize: Vector2
	OnScreenKeyboardAnimationDuration: number
	OnScreenKeyboardPosition: Vector2
	OnScreenKeyboardSize: Vector2
	OnScreenKeyboardVisible: boolean
	OverrideMouseIconBehavior: EnumOverrideMouseIconBehavior
	PointerAction: RBXScriptSignal<number, Vector2, number, boolean>
	RightBarSize: Vector2
	StatusBarSize: Vector2
	StatusBarTapped: RBXScriptSignal<Vector2>
	TextBoxFocusReleased: RBXScriptSignal<TextBox>
	TextBoxFocused: RBXScriptSignal<TextBox>
	TouchEnabled: boolean
	TouchEnded: RBXScriptSignal<InputObject, boolean>
	TouchLongPress: RBXScriptSignal<{ Vector2 }, EnumUserInputState, boolean>
	TouchMoved: RBXScriptSignal<InputObject, boolean>
	TouchPan: RBXScriptSignal<{ Vector2 }, Vector2, Vector2, EnumUserInputState, boolean>
	TouchPinch: RBXScriptSignal<{ Vector2 }, number, number, EnumUserInputState, boolean>
	TouchRotate: RBXScriptSignal<{ Vector2 }, number, number, EnumUserInputState, boolean>
	TouchStarted: RBXScriptSignal<InputObject, boolean>
	TouchSwipe: RBXScriptSignal<EnumSwipeDirection, number, boolean>
	TouchTap: RBXScriptSignal<{ Vector2 }, boolean>
	TouchTapInWorld: RBXScriptSignal<Vector2, boolean>
	UserCFrameChanged: RBXScriptSignal<EnumUserCFrame, CFrame>
	VREnabled: boolean
	WindowFocusReleased: RBXScriptSignal<>
	WindowFocused: RBXScriptSignal<>
	function GamepadSupports(self, gamepadNum: EnumUserInputType, gamepadKeyCode: EnumKeyCode): boolean
	function GetConnectedGamepads(self): { EnumUserInputType }
	function GetDeviceAcceleration(self): InputObject
	function GetDeviceGravity(self): InputObject
	function GetDeviceRotation(self): (number, CFrame)
	function GetDeviceType(self): EnumDeviceType
	function GetFocusedTextBox(self): TextBox
	function GetGamepadConnected(self, gamepadNum: EnumUserInputType): boolean
	function GetGamepadState(self, gamepadNum: EnumUserInputType): { InputObject }
	function GetKeysPressed(self): { InputObject }
	function GetLastInputType(self): EnumUserInputType
	function GetMouseButtonsPressed(self): { InputObject }
	function GetMouseDelta(self): Vector2
	function GetMouseLocation(self): Vector2
	function GetNavigationGamepads(self): { EnumUserInputType }
	function GetPlatform(self): EnumPlatform
	function GetStringForKeyCode(self, keyCode: EnumKeyCode): string
	function GetSupportedGamepadKeyCodes(self, gamepadNum: EnumUserInputType): { EnumKeyCode }
	function GetUserCFrame(self, type: EnumUserCFrame): CFrame
	function IsGamepadButtonDown(self, gamepadNum: EnumUserInputType, gamepadKeyCode: EnumKeyCode): boolean
	function IsKeyDown(self, keyCode: EnumKeyCode): boolean
	function IsMouseButtonPressed(self, mouseButton: EnumUserInputType): boolean
	function IsNavigationGamepad(self, gamepadEnum: EnumUserInputType): boolean
	function RecenterUserHeadCFrame(self): nil
	function SendAppUISizes(self, statusBarSize: Vector2, navBarSize: Vector2, bottomBarSize: Vector2, rightBarSize: Vector2): nil
	function SetNavigationGamepad(self, gamepadEnum: EnumUserInputType, enabled: boolean): nil
end

declare class UserService extends Instance
	function GetUserInfosByUserIdsAsync(self, userIds: { number }): { { Id: number, Username: string, DisplayName: string } }
end

declare class VRService extends Instance
	AutomaticScaling: EnumVRScaling
	DidPointerHit: boolean
	FadeOutViewOnCollision: boolean
	GuiInputUserCFrame: EnumUserCFrame
	LaserDistance: number
	LaserPointerTriggered: RBXScriptSignal<InputObject>
	NavigationRequested: RBXScriptSignal<CFrame, EnumUserCFrame>
	PointerHitCFrame: CFrame
	TouchpadModeChanged: RBXScriptSignal<EnumVRTouchpad, EnumVRTouchpadMode>
	UserCFrameChanged: RBXScriptSignal<EnumUserCFrame, CFrame>
	UserCFrameEnabled: RBXScriptSignal<EnumUserCFrame, boolean>
	VRDeviceAvailable: boolean
	VRDeviceName: string
	VREnabled: boolean
	VRSessionState: EnumVRSessionState
	function GetTouchpadMode(self, pad: EnumVRTouchpad): EnumVRTouchpadMode
	function GetUserCFrame(self, type: EnumUserCFrame): CFrame
	function GetUserCFrameEnabled(self, type: EnumUserCFrame): boolean
	function IsMaquettes(self): boolean
	function IsVRAppBuild(self): boolean
	function RecenterUserHeadCFrame(self): nil
	function RequestNavigation(self, cframe: CFrame, inputUserCFrame: EnumUserCFrame): nil
	function SetTouchpadMode(self, pad: EnumVRTouchpad, mode: EnumVRTouchpadMode): nil
end

declare class VRStatusService extends Instance
end

declare class ValueBase extends Instance
end

declare class BinaryStringValue extends ValueBase
	Changed: RBXScriptSignal<BinaryString>
end

declare class BoolValue extends ValueBase
	Changed: RBXScriptSignal<boolean>
	Value: boolean
end

declare class BrickColorValue extends ValueBase
	Changed: RBXScriptSignal<BrickColor>
	Value: BrickColor
end

declare class CFrameValue extends ValueBase
	Changed: RBXScriptSignal<CFrame>
	Value: CFrame
end

declare class Color3Value extends ValueBase
	Changed: RBXScriptSignal<Color3>
	Value: Color3
end





declare class IntValue extends ValueBase
	Changed: RBXScriptSignal<number>
	Value: number
end

declare class NumberValue extends ValueBase
	Changed: RBXScriptSignal<number>
	Value: number
end

declare class ObjectValue extends ValueBase
	Changed: RBXScriptSignal<Instance?>
	Value: Instance?
end

declare class RayValue extends ValueBase
	Changed: RBXScriptSignal<Ray>
	Value: Ray
end

declare class StringValue extends ValueBase
	Changed: RBXScriptSignal<string>
	Value: string
end

declare class Vector3Value extends ValueBase
	Changed: RBXScriptSignal<Vector3>
	Value: Vector3
end

declare class Vector3Curve extends Instance
	function GetValueAtTime(self, time: number): { any }
	function X(self): FloatCurve
	function Y(self): FloatCurve
	function Z(self): FloatCurve
end

declare class VersionControlService extends Instance
	ScriptCollabEnabled: boolean
end

declare class VideoCaptureService extends Instance
	Active: boolean
	CameraID: string
	DevicesChanged: RBXScriptSignal<>
	Error: RBXScriptSignal<string, string>
	Started: RBXScriptSignal<string>
	Stopped: RBXScriptSignal<string>
	function GetCameraDevices(self): { [any]: any }
end

declare class VideoService extends Instance
end

declare class VirtualInputManager extends Instance
	AdditionalLuaState: string
	PlaybackCompleted: RBXScriptSignal<string>
	RecordingCompleted: RBXScriptSignal<string>
	function Dump(self): nil
	function HandleGamepadAxisInput(self, objectId: number, keyCode: EnumKeyCode, x: number, y: number, z: number): nil
	function HandleGamepadButtonInput(self, deviceId: number, keyCode: EnumKeyCode, buttonState: number): nil
	function HandleGamepadConnect(self, deviceId: number): nil
	function HandleGamepadDisconnect(self, deviceId: number): nil
	function SendAccelerometerEvent(self, x: number, y: number, z: number): nil
	function SendGravityEvent(self, x: number, y: number, z: number): nil
	function SendGyroscopeEvent(self, quatX: number, quatY: number, quatZ: number, quatW: number): nil
	function SendKeyEvent(self, isPressed: boolean, keyCode: EnumKeyCode, isRepeatedKey: boolean, layerCollector: Instance): nil
	function SendMouseButtonEvent(self, x: number, y: number, mouseButton: number, isDown: boolean, layerCollector: Instance, repeatCount: number): nil
	function SendMouseMoveEvent(self, x: number, y: number, layerCollector: Instance): nil
	function SendMouseWheelEvent(self, x: number, y: number, isForwardScroll: boolean, layerCollector: Instance): nil
	function SendTextInputCharacterEvent(self, str: string, layerCollector: Instance): nil
	function SendTouchEvent(self, touchId: number, state: number, x: number, y: number): nil
	function SetInputTypesToIgnore(self, inputTypesToIgnore: any): nil
	function StartPlaying(self, fileName: string): nil
	function StartPlayingJSON(self, string: string): nil
	function StartRecording(self): nil
	function StopPlaying(self): nil
	function StopRecording(self): nil
	function WaitForInputEventsProcessed(self): nil
	function sendRobloxEvent(self, namespace: string, detail: string, detailType: string): nil
	function sendThemeChangeEvent(self, themeName: string): nil
end

declare class VirtualUser extends Instance
	function Button1Down(self, position: Vector2, camera: CFrame?): nil
	function Button1Up(self, position: Vector2, camera: CFrame?): nil
	function Button2Down(self, position: Vector2, camera: CFrame?): nil
	function Button2Up(self, position: Vector2, camera: CFrame?): nil
	function CaptureController(self): nil
	function ClickButton1(self, position: Vector2, camera: CFrame?): nil
	function ClickButton2(self, position: Vector2, camera: CFrame?): nil
	function MoveMouse(self, position: Vector2, camera: CFrame?): nil
	function SetKeyDown(self, key: string): nil
	function SetKeyUp(self, key: string): nil
	function StartRecording(self): nil
	function StopRecording(self): string
	function TypeKey(self, key: string): nil
end

declare class VisibilityCheckDispatcher extends Instance
end

declare class VisibilityService extends Instance
end

declare class Visit extends Instance
end

declare class VoiceChatInternal extends Instance
	LocalPlayerModerated: RBXScriptSignal<>
	function GetChannelId(self): string
	function GetGroupId(self): string
	function GetSessionId(self): string
	function GetVoiceExperienceId(self): string
	function IsContextVoiceEnabled(self): boolean
	function IsVoiceEnabledForUserIdAsync(self, userId: number): boolean
	function SubscribeBlock(self, userId: number): boolean
	function SubscribeRetry(self, userId: number): boolean
	function SubscribeUnblock(self, userId: number): boolean
end

declare class VoiceChatService extends Instance
	EnableDefaultVoice: boolean
	UseNewJoinFlow: boolean
	VoiceChatEnabledForPlaceOnRcc: boolean
	VoiceChatEnabledForUniverseOnRcc: boolean
	function IsVoiceEnabledForUserIdAsync(self, userId: number): boolean
end

declare class WeldConstraint extends Instance
	Active: boolean
	Enabled: boolean
	Part0: BasePart
	Part1: BasePart
end

declare Instance: {
	new: ((className: "Accoutrement", parent: Instance?) -> Accoutrement) & ((className: "Accessory", parent: Instance?) -> Accessory) & ((className: "Hat", parent: Instance?) -> Hat) & ((className: "AdPortal", parent: Instance?) -> AdPortal) & ((className: "AdvancedDragger", parent: Instance?) -> AdvancedDragger) & ((className: "AnalyticsService", parent: Instance?) -> AnalyticsService) & ((className: "Animation", parent: Instance?) -> Animation) & ((className: "CurveAnimation", parent: Instance?) -> CurveAnimation) & ((className: "KeyframeSequence", parent: Instance?) -> KeyframeSequence) & ((className: "AnimationController", parent: Instance?) -> AnimationController) & ((className: "AnimationRigData", parent: Instance?) -> AnimationRigData) & ((className: "Animator", parent: Instance?) -> Animator) & ((className: "Atmosphere", parent: Instance?) -> Atmosphere) & ((className: "Attachment", parent: Instance?) -> Attachment) & ((className: "Bone", parent: Instance?) -> Bone) & ((className: "AudioSearchParams", parent: Instance?) -> AudioSearchParams) & ((className: "Backpack", parent: Instance?) -> Backpack) & ((className: "WrapLayer", parent: Instance?) -> WrapLayer) & ((className: "WrapTarget", parent: Instance?) -> WrapTarget) & ((className: "Beam", parent: Instance?) -> Beam) & ((className: "BindableEvent", parent: Instance?) -> BindableEvent) & ((className: "BindableFunction", parent: Instance?) -> BindableFunction) & ((className: "BodyAngularVelocity", parent: Instance?) -> BodyAngularVelocity) & ((className: "BodyForce", parent: Instance?) -> BodyForce) & ((className: "BodyGyro", parent: Instance?) -> BodyGyro) & ((className: "BodyPosition", parent: Instance?) -> BodyPosition) & ((className: "BodyThrust", parent: Instance?) -> BodyThrust) & ((className: "BodyVelocity", parent: Instance?) -> BodyVelocity) & ((className: "RocketPropulsion", parent: Instance?) -> RocketPropulsion) & ((className: "Breakpoint", parent: Instance?) -> Breakpoint) & ((className: "BubbleChatMessageProperties", parent: Instance?) -> BubbleChatMessageProperties) & ((className: "Camera", parent: Instance?) -> Camera) & ((className: "BodyColors", parent: Instance?) -> BodyColors) & ((className: "CharacterMesh", parent: Instance?) -> CharacterMesh) & ((className: "Pants", parent: Instance?) -> Pants) & ((className: "Shirt", parent: Instance?) -> Shirt) & ((className: "ShirtGraphic", parent: Instance?) -> ShirtGraphic) & ((className: "Skin", parent: Instance?) -> Skin) & ((className: "ClickDetector", parent: Instance?) -> ClickDetector) & ((className: "DragDetector", parent: Instance?) -> DragDetector) & ((className: "Clouds", parent: Instance?) -> Clouds) & ((className: "Configuration", parent: Instance?) -> Configuration) & ((className: "AlignOrientation", parent: Instance?) -> AlignOrientation) & ((className: "AlignPosition", parent: Instance?) -> AlignPosition) & ((className: "AngularVelocity", parent: Instance?) -> AngularVelocity) & ((className: "AnimationConstraint", parent: Instance?) -> AnimationConstraint) & ((className: "BallSocketConstraint", parent: Instance?) -> BallSocketConstraint) & ((className: "HingeConstraint", parent: Instance?) -> HingeConstraint) & ((className: "LineForce", parent: Instance?) -> LineForce) & ((className: "LinearVelocity", parent: Instance?) -> LinearVelocity) & ((className: "PlaneConstraint", parent: Instance?) -> PlaneConstraint) & ((className: "Plane", parent: Instance?) -> Plane) & ((className: "RigidConstraint", parent: Instance?) -> RigidConstraint) & ((className: "RodConstraint", parent: Instance?) -> RodConstraint) & ((className: "RopeConstraint", parent: Instance?) -> RopeConstraint) & ((className: "CylindricalConstraint", parent: Instance?) -> CylindricalConstraint) & ((className: "PrismaticConstraint", parent: Instance?) -> PrismaticConstraint) & ((className: "SpringConstraint", parent: Instance?) -> SpringConstraint) & ((className: "Torque", parent: Instance?) -> Torque) & ((className: "TorsionSpringConstraint", parent: Instance?) -> TorsionSpringConstraint) & ((className: "UniversalConstraint", parent: Instance?) -> UniversalConstraint) & ((className: "VectorForce", parent: Instance?) -> VectorForce) & ((className: "HumanoidController", parent: Instance?) -> HumanoidController) & ((className: "SkateboardController", parent: Instance?) -> SkateboardController) & ((className: "VehicleController", parent: Instance?) -> VehicleController) & ((className: "AirController", parent: Instance?) -> AirController) & ((className: "ClimbController", parent: Instance?) -> ClimbController) & ((className: "GroundController", parent: Instance?) -> GroundController) & ((className: "SwimController", parent: Instance?) -> SwimController) & ((className: "ControllerManager", parent: Instance?) -> ControllerManager) & ((className: "CustomEvent", parent: Instance?) -> CustomEvent) & ((className: "CustomEventReceiver", parent: Instance?) -> CustomEventReceiver) & ((className: "CylinderMesh", parent: Instance?) -> CylinderMesh) & ((className: "DynamicMesh", parent: Instance?) -> DynamicMesh) & ((className: "FileMesh", parent: Instance?) -> FileMesh) & ((className: "SpecialMesh", parent: Instance?) -> SpecialMesh) & ((className: "DataStoreIncrementOptions", parent: Instance?) -> DataStoreIncrementOptions) & ((className: "DataStoreOptions", parent: Instance?) -> DataStoreOptions) & ((className: "DataStoreSetOptions", parent: Instance?) -> DataStoreSetOptions) & ((className: "DebuggerWatch", parent: Instance?) -> DebuggerWatch) & ((className: "Dialog", parent: Instance?) -> Dialog) & ((className: "DialogChoice", parent: Instance?) -> DialogChoice) & ((className: "Dragger", parent: Instance?) -> Dragger) & ((className: "EulerRotationCurve", parent: Instance?) -> EulerRotationCurve) & ((className: "ExperienceInviteOptions", parent: Instance?) -> ExperienceInviteOptions) & ((className: "Explosion", parent: Instance?) -> Explosion) & ((className: "FaceControls", parent: Instance?) -> FaceControls) & ((className: "Decal", parent: Instance?) -> Decal) & ((className: "Texture", parent: Instance?) -> Texture) & ((className: "Hole", parent: Instance?) -> Hole) & ((className: "MotorFeature", parent: Instance?) -> MotorFeature) & ((className: "Fire", parent: Instance?) -> Fire) & ((className: "FloatCurve", parent: Instance?) -> FloatCurve) & ((className: "FlyweightService", parent: Instance?) -> FlyweightService) & ((className: "CSGDictionaryService", parent: Instance?) -> CSGDictionaryService) & ((className: "NonReplicatedCSGDictionaryService", parent: Instance?) -> NonReplicatedCSGDictionaryService) & ((className: "Folder", parent: Instance?) -> Folder) & ((className: "ForceField", parent: Instance?) -> ForceField) & ((className: "FunctionalTest", parent: Instance?) -> FunctionalTest) & ((className: "GetTextBoundsParams", parent: Instance?) -> GetTextBoundsParams) & ((className: "CanvasGroup", parent: Instance?) -> CanvasGroup) & ((className: "Frame", parent: Instance?) -> Frame) & ((className: "ImageButton", parent: Instance?) -> ImageButton) & ((className: "TextButton", parent: Instance?) -> TextButton) & ((className: "ImageLabel", parent: Instance?) -> ImageLabel) & ((className: "TextLabel", parent: Instance?) -> TextLabel) & ((className: "ScrollingFrame", parent: Instance?) -> ScrollingFrame) & ((className: "TextBox", parent: Instance?) -> TextBox) & ((className: "VideoFrame", parent: Instance?) -> VideoFrame) & ((className: "ViewportFrame", parent: Instance?) -> ViewportFrame) & ((className: "BillboardGui", parent: Instance?) -> BillboardGui) & ((className: "ScreenGui", parent: Instance?) -> ScreenGui) & ((className: "GuiMain", parent: Instance?) -> GuiMain) & ((className: "AdGui", parent: Instance?) -> AdGui) & ((className: "SurfaceGui", parent: Instance?) -> SurfaceGui) & ((className: "FloorWire", parent: Instance?) -> FloorWire) & ((className: "SelectionBox", parent: Instance?) -> SelectionBox) & ((className: "BoxHandleAdornment", parent: Instance?) -> BoxHandleAdornment) & ((className: "ConeHandleAdornment", parent: Instance?) -> ConeHandleAdornment) & ((className: "CylinderHandleAdornment", parent: Instance?) -> CylinderHandleAdornment) & ((className: "ImageHandleAdornment", parent: Instance?) -> ImageHandleAdornment) & ((className: "LineHandleAdornment", parent: Instance?) -> LineHandleAdornment) & ((className: "SphereHandleAdornment", parent: Instance?) -> SphereHandleAdornment) & ((className: "WireframeHandleAdornment", parent: Instance?) -> WireframeHandleAdornment) & ((className: "ParabolaAdornment", parent: Instance?) -> ParabolaAdornment) & ((className: "SelectionSphere", parent: Instance?) -> SelectionSphere) & ((className: "ArcHandles", parent: Instance?) -> ArcHandles) & ((className: "Handles", parent: Instance?) -> Handles) & ((className: "SurfaceSelection", parent: Instance?) -> SurfaceSelection) & ((className: "SelectionPartLasso", parent: Instance?) -> SelectionPartLasso) & ((className: "SelectionPointLasso", parent: Instance?) -> SelectionPointLasso) & ((className: "HeightmapImporterService", parent: Instance?) -> HeightmapImporterService) & ((className: "HiddenSurfaceRemovalAsset", parent: Instance?) -> HiddenSurfaceRemovalAsset) & ((className: "Highlight", parent: Instance?) -> Highlight) & ((className: "Humanoid", parent: Instance?) -> Humanoid) & ((className: "HumanoidDescription", parent: Instance?) -> HumanoidDescription) & ((className: "IKControl", parent: Instance?) -> IKControl) & ((className: "RotateP", parent: Instance?) -> RotateP) & ((className: "RotateV", parent: Instance?) -> RotateV) & ((className: "Glue", parent: Instance?) -> Glue) & ((className: "ManualGlue", parent: Instance?) -> ManualGlue) & ((className: "ManualWeld", parent: Instance?) -> ManualWeld) & ((className: "Motor", parent: Instance?) -> Motor) & ((className: "Motor6D", parent: Instance?) -> Motor6D) & ((className: "Rotate", parent: Instance?) -> Rotate) & ((className: "Snap", parent: Instance?) -> Snap) & ((className: "VelocityMotor", parent: Instance?) -> VelocityMotor) & ((className: "Weld", parent: Instance?) -> Weld) & ((className: "Keyframe", parent: Instance?) -> Keyframe) & ((className: "KeyframeMarker", parent: Instance?) -> KeyframeMarker) & ((className: "PointLight", parent: Instance?) -> PointLight) & ((className: "SpotLight", parent: Instance?) -> SpotLight) & ((className: "SurfaceLight", parent: Instance?) -> SurfaceLight) & ((className: "LocalizationTable", parent: Instance?) -> LocalizationTable) & ((className: "Script", parent: Instance?) -> Script) & ((className: "LocalScript", parent: Instance?) -> LocalScript) & ((className: "ModuleScript", parent: Instance?) -> ModuleScript) & ((className: "MarkerCurve", parent: Instance?) -> MarkerCurve) & ((className: "MaterialVariant", parent: Instance?) -> MaterialVariant) & ((className: "MemoryStoreService", parent: Instance?) -> MemoryStoreService) & ((className: "Message", parent: Instance?) -> Message) & ((className: "Hint", parent: Instance?) -> Hint) & ((className: "NoCollisionConstraint", parent: Instance?) -> NoCollisionConstraint) & ((className: "CornerWedgePart", parent: Instance?) -> CornerWedgePart) & ((className: "Part", parent: Instance?) -> Part) & ((className: "FlagStand", parent: Instance?) -> FlagStand) & ((className: "Seat", parent: Instance?) -> Seat) & ((className: "SkateboardPlatform", parent: Instance?) -> SkateboardPlatform) & ((className: "SpawnLocation", parent: Instance?) -> SpawnLocation) & ((className: "WedgePart", parent: Instance?) -> WedgePart) & ((className: "MeshPart", parent: Instance?) -> MeshPart) & ((className: "PartOperation", parent: Instance?) -> PartOperation) & ((className: "IntersectOperation", parent: Instance?) -> IntersectOperation) & ((className: "NegateOperation", parent: Instance?) -> NegateOperation) & ((className: "UnionOperation", parent: Instance?) -> UnionOperation) & ((className: "TrussPart", parent: Instance?) -> TrussPart) & ((className: "VehicleSeat", parent: Instance?) -> VehicleSeat) & ((className: "Model", parent: Instance?) -> Model) & ((className: "Actor", parent: Instance?) -> Actor) & ((className: "HopperBin", parent: Instance?) -> HopperBin) & ((className: "Tool", parent: Instance?) -> Tool) & ((className: "Flag", parent: Instance?) -> Flag) & ((className: "WorldModel", parent: Instance?) -> WorldModel) & ((className: "PartOperationAsset", parent: Instance?) -> PartOperationAsset) & ((className: "ParticleEmitter", parent: Instance?) -> ParticleEmitter) & ((className: "PathfindingLink", parent: Instance?) -> PathfindingLink) & ((className: "PathfindingModifier", parent: Instance?) -> PathfindingModifier) & ((className: "Player", parent: Instance?) -> Player) & ((className: "PluginAction", parent: Instance?) -> PluginAction) & ((className: "PluginCapabilities", parent: Instance?) -> PluginCapabilities) & ((className: "NumberPose", parent: Instance?) -> NumberPose) & ((className: "Pose", parent: Instance?) -> Pose) & ((className: "BloomEffect", parent: Instance?) -> BloomEffect) & ((className: "BlurEffect", parent: Instance?) -> BlurEffect) & ((className: "ColorCorrectionEffect", parent: Instance?) -> ColorCorrectionEffect) & ((className: "DepthOfFieldEffect", parent: Instance?) -> DepthOfFieldEffect) & ((className: "SunRaysEffect", parent: Instance?) -> SunRaysEffect) & ((className: "ProximityPrompt", parent: Instance?) -> ProximityPrompt) & ((className: "ProximityPromptService", parent: Instance?) -> ProximityPromptService) & ((className: "ReflectionMetadata", parent: Instance?) -> ReflectionMetadata) & ((className: "ReflectionMetadataCallbacks", parent: Instance?) -> ReflectionMetadataCallbacks) & ((className: "ReflectionMetadataClasses", parent: Instance?) -> ReflectionMetadataClasses) & ((className: "ReflectionMetadataEnums", parent: Instance?) -> ReflectionMetadataEnums) & ((className: "ReflectionMetadataEvents", parent: Instance?) -> ReflectionMetadataEvents) & ((className: "ReflectionMetadataFunctions", parent: Instance?) -> ReflectionMetadataFunctions) & ((className: "ReflectionMetadataClass", parent: Instance?) -> ReflectionMetadataClass) & ((className: "ReflectionMetadataEnum", parent: Instance?) -> ReflectionMetadataEnum) & ((className: "ReflectionMetadataEnumItem", parent: Instance?) -> ReflectionMetadataEnumItem) & ((className: "ReflectionMetadataMember", parent: Instance?) -> ReflectionMetadataMember) & ((className: "ReflectionMetadataProperties", parent: Instance?) -> ReflectionMetadataProperties) & ((className: "ReflectionMetadataYieldFunctions", parent: Instance?) -> ReflectionMetadataYieldFunctions) & ((className: "RemoteEvent", parent: Instance?) -> RemoteEvent) & ((className: "RemoteFunction", parent: Instance?) -> RemoteFunction) & ((className: "RenderingTest", parent: Instance?) -> RenderingTest) & ((className: "RotationCurve", parent: Instance?) -> RotationCurve) & ((className: "BuoyancySensor", parent: Instance?) -> BuoyancySensor) & ((className: "ControllerPartSensor", parent: Instance?) -> ControllerPartSensor) & ((className: "Sky", parent: Instance?) -> Sky) & ((className: "Smoke", parent: Instance?) -> Smoke) & ((className: "Sound", parent: Instance?) -> Sound) & ((className: "ChorusSoundEffect", parent: Instance?) -> ChorusSoundEffect) & ((className: "CompressorSoundEffect", parent: Instance?) -> CompressorSoundEffect) & ((className: "DistortionSoundEffect", parent: Instance?) -> DistortionSoundEffect) & ((className: "EchoSoundEffect", parent: Instance?) -> EchoSoundEffect) & ((className: "EqualizerSoundEffect", parent: Instance?) -> EqualizerSoundEffect) & ((className: "FlangeSoundEffect", parent: Instance?) -> FlangeSoundEffect) & ((className: "PitchShiftSoundEffect", parent: Instance?) -> PitchShiftSoundEffect) & ((className: "ReverbSoundEffect", parent: Instance?) -> ReverbSoundEffect) & ((className: "TremoloSoundEffect", parent: Instance?) -> TremoloSoundEffect) & ((className: "SoundGroup", parent: Instance?) -> SoundGroup) & ((className: "Sparkles", parent: Instance?) -> Sparkles) & ((className: "StandalonePluginScripts", parent: Instance?) -> StandalonePluginScripts) & ((className: "StarterGear", parent: Instance?) -> StarterGear) & ((className: "StudioCallout", parent: Instance?) -> StudioCallout) & ((className: "StudioObjectBase", parent: Instance?) -> StudioObjectBase) & ((className: "StudioWidget", parent: Instance?) -> StudioWidget) & ((className: "StyleRule", parent: Instance?) -> StyleRule) & ((className: "StyleSheet", parent: Instance?) -> StyleSheet) & ((className: "StyleDerive", parent: Instance?) -> StyleDerive) & ((className: "StyleLink", parent: Instance?) -> StyleLink) & ((className: "SurfaceAppearance", parent: Instance?) -> SurfaceAppearance) & ((className: "Team", parent: Instance?) -> Team) & ((className: "TeleportOptions", parent: Instance?) -> TeleportOptions) & ((className: "TerrainDetail", parent: Instance?) -> TerrainDetail) & ((className: "TerrainRegion", parent: Instance?) -> TerrainRegion) & ((className: "TestService", parent: Instance?) -> TestService) & ((className: "TextChannel", parent: Instance?) -> TextChannel) & ((className: "TextChatCommand", parent: Instance?) -> TextChatCommand) & ((className: "TextChatMessageProperties", parent: Instance?) -> TextChatMessageProperties) & ((className: "TrackerStreamAnimation", parent: Instance?) -> TrackerStreamAnimation) & ((className: "Trail", parent: Instance?) -> Trail) & ((className: "Tween", parent: Instance?) -> Tween) & ((className: "UIAspectRatioConstraint", parent: Instance?) -> UIAspectRatioConstraint) & ((className: "UISizeConstraint", parent: Instance?) -> UISizeConstraint) & ((className: "UITextSizeConstraint", parent: Instance?) -> UITextSizeConstraint) & ((className: "UICorner", parent: Instance?) -> UICorner) & ((className: "UIGradient", parent: Instance?) -> UIGradient) & ((className: "UIGridLayout", parent: Instance?) -> UIGridLayout) & ((className: "UIListLayout", parent: Instance?) -> UIListLayout) & ((className: "UIPageLayout", parent: Instance?) -> UIPageLayout) & ((className: "UITableLayout", parent: Instance?) -> UITableLayout) & ((className: "UIPadding", parent: Instance?) -> UIPadding) & ((className: "UIScale", parent: Instance?) -> UIScale) & ((className: "UIStroke", parent: Instance?) -> UIStroke) & ((className: "BinaryStringValue", parent: Instance?) -> BinaryStringValue) & ((className: "BoolValue", parent: Instance?) -> BoolValue) & ((className: "BrickColorValue", parent: Instance?) -> BrickColorValue) & ((className: "CFrameValue", parent: Instance?) -> CFrameValue) & ((className: "Color3Value", parent: Instance?) -> Color3Value) & ((className: "DoubleConstrainedValue", parent: Instance?) -> DoubleConstrainedValue) & ((className: "IntConstrainedValue", parent: Instance?) -> IntConstrainedValue) & ((className: "IntValue", parent: Instance?) -> IntValue) & ((className: "NumberValue", parent: Instance?) -> NumberValue) & ((className: "ObjectValue", parent: Instance?) -> ObjectValue) & ((className: "RayValue", parent: Instance?) -> RayValue) & ((className: "StringValue", parent: Instance?) -> StringValue) & ((className: "Vector3Value", parent: Instance?) -> Vector3Value) & ((className: "Vector3Curve", parent: Instance?) -> Vector3Curve) & ((className: "VirtualInputManager", parent: Instance?) -> VirtualInputManager) & ((className: "WeldConstraint", parent: Instance?) -> WeldConstraint) & ((className: string, parent: Instance?) -> Instance),
}

declare Ray: {
	new: ((Origin: Vector3, Direction: Vector3) -> Ray),
}

declare NumberRange: {
	new: ((value: number) -> NumberRange) & ((min: number, max: number) -> NumberRange),
}

declare PathWaypoint: {
	new: ((position: Vector3, action: EnumPathWaypointAction) -> PathWaypoint),
}

declare BrickColor: {
	Red: (() -> BrickColor),
	Yellow: (() -> BrickColor),
	Blue: (() -> BrickColor),
	Gray: (() -> BrickColor),
	DarkGray: (() -> BrickColor),
	White: (() -> BrickColor),
	random: (() -> BrickColor),
	Green: (() -> BrickColor),
	Black: (() -> BrickColor),
	palette: ((paletteValue: number) -> BrickColor),
	new: ((val: number) -> BrickColor) & ((r: number, g: number, b: number) -> BrickColor) & ((color: Color3) -> BrickColor) & ((name: "Alder" | "Artichoke" | "Baby blue" | "Beige" | "Black" | "Black metallic" | "Br. reddish orange" | "Br. yellowish green" | "Br. yellowish orange" | "Brick yellow" | "Bright blue" | "Bright bluish green" | "Bright bluish violet" | "Bright green" | "Bright orange" | "Bright purple" | "Bright red" | "Bright reddish lilac" | "Bright reddish violet" | "Bright violet" | "Bright yellow" | "Bronze" | "Brown" | "Burgundy" | "Burlap" | "Burnt Sienna" | "Buttermilk" | "CGA brown" | "Cadet blue" | "Camo" | "Carnation pink" | "Cashmere" | "Cloudy grey" | "Cocoa" | "Cool yellow" | "Copper" | "Cork" | "Crimson" | "Curry" | "Cyan" | "Daisy orange" | "Dark Curry" | "Dark Royal blue" | "Dark blue" | "Dark green" | "Dark grey" | "Dark grey metallic" | "Dark indigo" | "Dark nougat" | "Dark orange" | "Dark red" | "Dark stone grey" | "Dark taupe" | "Deep blue" | "Deep orange" | "Dirt brown" | "Dove blue" | "Dusty Rose" | "Earth blue" | "Earth green" | "Earth orange" | "Earth yellow" | "Eggplant" | "Electric blue" | "Faded green" | "Fawn brown" | "Fire Yellow" | "Flame reddish orange" | "Flame yellowish orange" | "Flint" | "Fog" | "Forest green" | "Fossil" | "Ghost grey" | "Gold" | "Grey" | "Grime" | "Gun metallic" | "Hot pink" | "Hurricane grey" | "Institutional white" | "Khaki" | "Lapis" | "Laurel green" | "Lavender" | "Lemon metalic" | "Lig. Yellowich orange" | "Lig. yellowish green" | "Light Royal blue" | "Light blue" | "Light bluish green" | "Light bluish violet" | "Light brick yellow" | "Light green (Mint)" | "Light grey" | "Light grey metallic" | "Light lilac" | "Light orange" | "Light orange brown" | "Light pink" | "Light purple" | "Light red" | "Light reddish violet" | "Light stone grey" | "Light yellow" | "Lilac" | "Lily white" | "Lime green" | "Linen" | "Magenta" | "Maroon" | "Mauve" | "Med. bluish green" | "Med. reddish violet" | "Med. yellowish green" | "Med. yellowish orange" | "Medium Royal blue" | "Medium blue" | "Medium bluish violet" | "Medium green" | "Medium lilac" | "Medium orange" | "Medium red" | "Medium stone grey" | "Mid gray" | "Mint" | "Moss" | "Mulberry" | "Navy blue" | "Neon green" | "Neon orange" | "New Yeller" | "Nougat" | "Olive" | "Olivine" | "Oyster" | "Parsley green" | "Pastel Blue" | "Pastel blue-green" | "Pastel brown" | "Pastel green" | "Pastel light blue" | "Pastel orange" | "Pastel violet" | "Pastel yellow" | "Pearl" | "Persimmon" | "Phosph. White" | "Pine Cone" | "Pink" | "Plum" | "Quill grey" | "Really black" | "Really blue" | "Really red" | "Red flip/flop" | "Reddish brown" | "Reddish lilac" | "Royal blue" | "Royal purple" | "Rust" | "Sage green" | "Salmon" | "Sand blue" | "Sand blue metallic" | "Sand green" | "Sand red" | "Sand violet" | "Sand violet metallic" | "Sand yellow" | "Sand yellow metallic" | "Sea green" | "Seashell" | "Shamrock" | "Silver" | "Silver flip/flop" | "Slime green" | "Smoky grey" | "Steel blue" | "Storm blue" | "Sunrise" | "Tawny" | "Teal" | "Terra Cotta" | "Toothpaste" | "Tr. Blue" | "Tr. Bright bluish violet" | "Tr. Brown" | "Tr. Flu. Blue" | "Tr. Flu. Green" | "Tr. Flu. Red" | "Tr. Flu. Reddish orange" | "Tr. Flu. Yellow" | "Tr. Green" | "Tr. Lg blue" | "Tr. Medi. reddish violet" | "Tr. Red" | "Tr. Yellow" | "Transparent" | "Turquoise" | "Warm yellowish orange" | "Wheat" | "White" | "Yellow flip/flop") -> BrickColor),
}

declare Vector2: {
	zero: Vector2,
	one: Vector2,
	xAxis: Vector2,
	yAxis: Vector2,
	new: ((x: number?, y: number?) -> Vector2),
}

declare Vector2int16: {
	new: ((x: number, y: number) -> Vector2int16),
}

declare Color3: {
	fromRGB: ((red: number?, green: number?, blue: number?) -> Color3),
	fromHSV: ((hue: number, saturation: number, value: number) -> Color3),
	toHSV: ((color: Color3) -> (number, number, number)),
	new: ((red: number?, green: number?, blue: number?) -> Color3),
	fromHex: ((hex: string) -> Color3),
}

declare UDim: {
	new: ((Scale: number?, Offset: number?) -> UDim),
}

declare PhysicalProperties: {
	new: ((material: EnumMaterial) -> PhysicalProperties) & ((density: number, friction: number, elasticy: number, frictionWeight: number?, elasticyWeight: number?) -> PhysicalProperties),
}

declare Axes: {
	new: ((axes: any) -> Axes),
}

declare Region3: {
	new: ((min: Vector3, max: Vector3) -> Region3),
}

declare Region3int16: {
	new: ((min: Vector3int16, max: Vector3int16) -> Region3int16),
}

declare UDim2: {
	fromScale: ((xScale: number, yScale: number) -> UDim2),
	fromOffset: ((xOffset: number, yOffset: number) -> UDim2),
	new: ((x: UDim, y: UDim) -> UDim2) & ((xScale: number?, xOffset: number?, yScale: number?, yOffset: number?) -> UDim2),
}

declare CFrame: {
	identity: CFrame,
	fromEulerAnglesYXZ: ((rx: number, ry: number, rz: number) -> CFrame),
	Angles: ((rx: number, ry: number, rz: number) -> CFrame),
	fromMatrix: ((pos: Vector3, vX: Vector3, vY: Vector3, vZ: Vector3?) -> CFrame),
	fromAxisAngle: ((v: Vector3, r: number) -> CFrame),
	fromOrientation: ((rx: number, ry: number, rz: number) -> CFrame),
	fromEulerAnglesXYZ: ((rx: number, ry: number, rz: number) -> CFrame),
	lookAt: ((at: Vector3, target: Vector3, up: Vector3?) -> CFrame),
	new: (() -> CFrame) & ((pos: Vector3) -> CFrame) & ((pos: Vector3, lookAt: Vector3) -> CFrame) & ((x: number, y: number, z: number) -> CFrame) & ((x: number, y: number, z: number, qX: number, qY: number, qZ: number, qW: number) -> CFrame) & ((x: number, y: number, z: number, R00: number, R01: number, R02: number, R10: number, R11: number, R12: number, R20: number, R21: number, R22: number) -> CFrame),
}

declare Faces: {
	new: ((normalIds: any) -> Faces),
}

declare Rect: {
	new: ((min: Vector2, max: Vector2) -> Rect) & ((minX: number, minY: number, maxX: number, maxY: number) -> Rect),
}

declare Vector3: {
	zero: Vector3,
	one: Vector3,
	xAxis: Vector3,
	yAxis: Vector3,
	zAxis: Vector3,
	fromNormalId: ((normal: EnumNormalId) -> Vector3),
	fromAxis: ((axis: EnumAxis) -> Vector3),
	FromNormalId: ((normal: EnumNormalId) -> Vector3),
	FromAxis: ((axis: EnumAxis) -> Vector3),
	new: ((x: number?, y: number?, z: number?) -> Vector3),
}

declare Vector3int16: {
	new: ((x: number?, y: number?, z: number?) -> Vector3int16),
}

declare Random: {
	new: ((seed: number?) -> Random),
}

declare TweenInfo: {
	new: ((time: number?, easingStyle: EnumEasingStyle?, easingDirection: EnumEasingDirection?, repeatCount: number?, reverses: boolean?, delayTime: number?) -> TweenInfo),
}

declare DateTime: {
	now: (() -> DateTime),
	fromUnixTimestamp: ((unixTimestamp: number) -> DateTime),
	fromUnixTimestampMillis: ((unixTimestampMillis: number) -> DateTime),
	fromUniversalTime: ((year: number?, month: number?, day: number?, hour: number?, minute: number?, second: number?, millisecond: number?) -> DateTime),
	fromLocalTime: ((year: number?, month: number?, day: number?, hour: number?, minute: number?, second: number?, millisecond: number?) -> DateTime),
	fromIsoDate: ((isoDate: string) -> DateTime),
}

declare NumberSequence: {
	new: ((n: number) -> NumberSequence) & ((n0: number, n1: number) -> NumberSequence) & ((keypoints: { NumberSequenceKeypoint }) -> NumberSequence),
}

declare ColorSequence: {
	new: ((c: Color3) -> ColorSequence) & ((c0: Color3, c1: Color3) -> ColorSequence) & ((keypoints: { ColorSequenceKeypoint }) -> ColorSequence),
}

declare NumberSequenceKeypoint: {
	new: ((time: number, value: number, envelop: number?) -> NumberSequenceKeypoint),
}

declare ColorSequenceKeypoint: {
	new: ((time: number, color: Color3) -> ColorSequenceKeypoint),
}

declare RaycastParams: {
	new: (() -> RaycastParams),
}

declare OverlapParams: {
	new: (() -> OverlapParams),
}

declare DockWidgetPluginGuiInfo: {
	new: ((initDockState: EnumInitialDockState?, initEnabled: boolean?, overrideEnabledRestore: boolean?, floatXSize: number?, floatYSize: number?, minWidth: number?, minHeight: number?) -> DockWidgetPluginGuiInfo),
}

declare CatalogSearchParams: {
	new: (() -> CatalogSearchParams),
}

declare Font: {
	new: ((family: string, weight: EnumFontWeight?, style: EnumFontStyle?) -> Font),
	fromEnum: ((font: EnumFont) -> Font),
	fromName: ((name: string, weight: EnumFontWeight?, style: EnumFontStyle?) -> Font),
	fromId: ((id: number, weight: EnumFontWeight?, style: EnumFontStyle?) -> Font),
}

declare FloatCurveKey: {
	new: ((time: number, value: number, Interpolation: EnumKeyInterpolationMode) -> FloatCurveKey),
}

declare RotationCurveKey: {
	new: ((time: number, value: CFrame, Interpolation: EnumKeyInterpolationMode) -> RotationCurveKey),
}


declare class GlobalSettings extends GenericSettings
    Lua: LuaSettings
    Game: GameSettings
    Studio: Studio
    Network: NetworkSettings
    Physics: PhysicsSettings
    Rendering: RenderSettings
    Diagnostics: DebugSettings
    function GetFFlag(self, name: string): boolean
    function GetFVariable(self, name: string): string
end

declare SharedTable: {
    new: () -> SharedTable,
    new: (t: { [any]: any }) -> SharedTable,
    clear: (st: SharedTable) -> (),
    clone: (st: SharedTable, deep: boolean?) -> SharedTable,
    cloneAndFreeze: (st: SharedTable, deep: boolean?) -> SharedTable,
    increment: (st: SharedTable, key: string | number, delta: number) -> number,
    isFrozen: (st: SharedTable) -> boolean,
    size: (st: SharedTable) -> number,
    update: (st: SharedTable, key: string | number, f: (any) -> any) -> (),
}

declare game: DataModel
declare workspace: Workspace
declare plugin: Plugin
declare script: LuaSourceContainer
declare function settings(): GlobalSettings
declare function UserSettings(): UserSettings

