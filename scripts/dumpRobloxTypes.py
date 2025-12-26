# Script to pull in API Dump and export it into a definition file
# Based off https://gist.github.com/HawDevelopment/97f2411149e24d8e7a712016114d55ff
import re
from typing import List, Literal, Optional, Set, Union, TypedDict
from collections import defaultdict
import requests
import json
import sys

# API Endpoints
DATA_TYPES = open("DataTypes.json", "r")
CORRECTIONS = open("Corrections.json", "r")
API_DUMP_URL = "https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/Full-API-Dump.json"
LUAU_TYPES_URL = "https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/roblox/LuauTypes.d.luau"
BRICK_COLORS_URL = "https://gist.githubusercontent.com/Anaminus/49ac255a68e7a7bc3cdd72b602d5071f/raw/f1534dcae312dbfda716b7677f8ac338b565afc3/BrickColor.json"

# Whether to include deprecated members that cannot be assigned the @deprecated attribute (i.e. deprecated non-functions). 
# Deprecated functions will always be defined, with their corresponding @deprecated attribute.
INCLUDE_DEPRECATED_MEMBERS = False

SECURITY_LEVELS = [
    "None",
    "LocalUserSecurity",
    "PluginSecurity",
    "WritePlayerSecurity",
    "RobloxScriptSecurity",
    "RobloxSecurity",
    "NotAccessibleSecurity",
]

DEFAULT_SECURITY_LEVEL = "RobloxScriptSecurity"

# Labeled sections of the LuauTypes.d.luau to remove entirely
DELETED_LUAU_SECTIONS = [
    "TestEZ", # Implied to be injected into modules suffixed with .test
    "BehaviorScript", # Related to upcoming server authority changes, not usable currently.
]

# Manual corrections to the RobloxGlobals in LuauTypes.d.luau
# Also used as a fallback failsafe for some of the functions.
LUAU_SNIPPET_PATCHES = {
    "declare function delay(delay: number?, callback: () -> ())":
        "@[deprecated{ use = \"task.delay\" }]\ndeclare function delay(delay: number?, callback: (dt: number, gt: number) -> ())",
    
    "declare function collectgarbage(mode: string): number":
        "@[deprecated{ use = \"gcinfo\" }]\ndeclare function collectgarbage(mode: \"count\"): number",
    
    "declare function stats()":
        "@[deprecated{ use = 'game:GetService(\"Stats\")' }]\ndeclare function stats(): Stats",
    
    "declare function wait(delay: number?): (number, number)": 
        "@[deprecated{ use = \"task.wait\" }]\ndeclare function wait(delay: number?): (number, number)",
    
    "declare function printidentity(prefix: string?)":
        "@deprecated declare function printidentity(prefix: string?)",

    "declare function version(): string":
        "@deprecated declare function version(): string",
    
    "declare game: any": "declare game: DataModel",
    "declare workspace: any": "declare workspace: Workspace",
    "declare script: any": "declare script: LuaSourceContainer",

    "declare Delay: typeof(delay)": 
        "@[deprecated{ use = \"task.delay\" }]\ndeclare function Delay(delay: number?, callback: (dt: number, gt: number) -> ())",
    
    "declare Wait: typeof(wait)": 
        "@[deprecated{ use = \"task.wait\" }]\ndeclare function Wait(delay: number?): (number, number)",
    
    "declare ElapsedTime: typeof(elapsedTime)": 
        "@[deprecated{ use = \"elapsedTime\" }]\ndeclare function ElapsedTime(): number",
    
    "declare Stats: typeof(stats)":
        "@[deprecated{ use = 'game:GetService(\"Stats\")' }]\ndeclare function Stats(): Stats",
    
    "declare Version: typeof(version)": 
        "@[deprecated{ use = \"version\" }]\ndeclare function Version(): string",
    
    "declare Workspace: any": "",
    "declare Game: any": "",
}

TYPE_INDEX = {
    "Tuple": "any",
    "Variant": "any",
    "Function": "((...any) -> ...any)",
    "function": "((...any) -> ...any)",
    "bool": "boolean",
    "int": "number",
    "int64": "number",
    "double": "number",
    "float": "number",
    "void": "nil",
    "null": "nil",
    "Objects": "{ Instance }",
    "Instances": "{ Instance }",
    "Dictionary": "{ [string]: any }",
    "Map": "{ [any]: any }",
    "Array": "{ any }",
    "table": "{ any }",
    "CoordinateFrame": "CFrame",
    "OptionalCoordinateFrame": "CFrame?",
}

IGNORED_INSTANCES: List[str] = [
    "RBXScriptSignal",  # Redefined using generics
    "Enum",  # redefined explicitly
    "EnumItem",  # redefined explicitly
    "GlobalSettings",  # redefined explicitly
    "SharedTable",  # redefined explicitly as the RobloxLsp type is incomplete
    "RaycastResult",  # Redefined using generics
]

# Extra members to add in to classes, commonly used to add in metamethods, and add corrections
EXTRA_MEMBERS = {
    "Vector3": [
        "function Angle(self, other: Vector3, axis: Vector3?): number",
        "function __add(self, other: Vector3): Vector3",
        "function __sub(self, other: Vector3): Vector3",
        "function __mul(self, other: Vector3 | number): Vector3",
        "function __div(self, other: Vector3 | number): Vector3",
        "function __unm(self): Vector3",
        "function __idiv(self, other: Vector3 | number): Vector3",
    ],
    "Vector2": [
        "function FuzzyEq(self, other: Vector2, epsilon: number?): boolean",
        "function __add(self, other: Vector2): Vector2",
        "function __sub(self, other: Vector2): Vector2",
        "function __mul(self, other: Vector2 | number): Vector2",
        "function __div(self, other: Vector2 | number): Vector2",
        "function __idiv(self, other: Vector2 | number): Vector2",
        "function __unm(self): Vector2",
    ],
    "Vector3int16": [
        "function __add(self, other: Vector3int16): Vector3int16",
        "function __sub(self, other: Vector3int16): Vector3int16",
        "function __mul(self, other: Vector3int16 | number): Vector3int16",
        "function __div(self, other: Vector3int16 | number): Vector3int16",
        "function __unm(self): Vector3int16",
    ],
    "Vector2int16": [
        "function __add(self, other: Vector2int16): Vector2int16",
        "function __sub(self, other: Vector2int16): Vector2int16",
        "function __mul(self, other: Vector2int16 | number): Vector2int16",
        "function __div(self, other: Vector2int16 | number): Vector2int16",
        "function __unm(self): Vector2int16",
    ],
    "UDim2": [
        "function __add(self, other: UDim2): UDim2",
        "function __sub(self, other: UDim2): UDim2",
        "function __unm(self): UDim2",
    ],
    "UDim": [
        "function __add(self, other: UDim): UDim",
        "function __sub(self, other: UDim): UDim",
        "function __unm(self): UDim",
    ],
    "CFrame": [
        "function __add(self, other: Vector3): CFrame",
        "function __sub(self, other: Vector3): CFrame",
        "function __mul(self, other: CFrame): CFrame",
        "function __mul(self, other: Vector3): Vector3",
    ],
    "Random": [
        "function Shuffle(self, table: { any })",
    ],
    "UserSettings": [
        "GameSettings: UserGameSettings",
        'function GetService(self, service: "UserGameSettings"): UserGameSettings',
    ],
    "Object": [
        "function GetPropertyChangedSignal(self, property: string): RBXScriptSignal<>",
    ],
    "Instance": [
        "Parent: Instance?",
        "AncestryChanged: RBXScriptSignal<Instance, Instance?>",
        "function FindFirstAncestor(self, name: string): Instance?",
        "function FindFirstAncestorOfClass(self, className: string): Instance?",
        "function FindFirstAncestorWhichIsA(self, className: string): Instance?",
        "function FindFirstChild(self, name: string, recursive: boolean?): Instance?",
        "function FindFirstChildOfClass(self, className: string): Instance?",
        "function FindFirstChildWhichIsA(self, className: string, recursive: boolean?): Instance?",
        "function FindFirstDescendant(self, name: string): Instance?",
        "function GetActor(self): Actor?",
        "function WaitForChild(self, name: string): Instance",
        "function WaitForChild(self, name: string, timeout: number): Instance?",
        "function GetAttribute(self, attribute: string): unknown?",
        "function GetAttributes(self): { [string]: unknown }",
        "function GetAttributeChangedSignal(self, attribute: string): RBXScriptSignal<>",
    ],
    "Model": ["PrimaryPart: BasePart?"],
    "RemoteEvent": [
        "function FireAllClients(self, ...: any): ()",
        "function FireClient(self, player: Player, ...: any): ()",
        "function FireServer(self, ...: any): ()",
        "OnClientEvent: RBXScriptSignal<...any>",
        "OnServerEvent: RBXScriptSignal<(Player, ...any)>",
    ],
    "UnreliableRemoteEvent": [
        "function FireAllClients(self, ...: any): ()",
        "function FireClient(self, player: Player, ...: any): ()",
        "function FireServer(self, ...: any): ()",
        "OnClientEvent: RBXScriptSignal<...any>",
        "OnServerEvent: RBXScriptSignal<(Player, ...any)>",
    ],
    "RemoteFunction": [
        "function InvokeClient(self, player: Player, ...: any): ...any",
        "function InvokeServer(self, ...: any): ...any",
        "OnClientInvoke: (...any) -> ...any",
        "OnServerInvoke: (player: Player, ...any) -> ...any",
    ],
    "BindableEvent": [
        "function Fire(self, ...: any): ()",
        "Event: RBXScriptSignal<...any>",
    ],
    "BindableFunction": [
        "function Invoke(self, ...: any): ...any",
        "OnInvoke: (...any) -> ...any",
    ],
    "Players": [
        "PlayerChatted: RBXScriptSignal<EnumPlayerChatType, Player, string, Player?>",
        "function GetPlayerByUserId(self, userId: number): Player?",
        "function GetPlayerFromCharacter(self, character: Model): Player?",
    ],
    "ContextActionService": [
        "function BindAction(self, actionName: string, functionToBind: (actionName: string, inputState: EnumUserInputState, inputObject: InputObject) -> EnumContextActionResult?, createTouchButton: boolean, ...: EnumUserInputType | EnumKeyCode): ()",
        "function BindActionAtPriority(self, actionName: string, functionToBind: (actionName: string, inputState: EnumUserInputState, inputObject: InputObject) -> EnumContextActionResult?, createTouchButton: boolean, priorityLevel: number, ...: EnumUserInputType | EnumKeyCode): ()",
    ],
    "Plugin": [
        "function OpenScript(self, script: LuaSourceContainer, lineNumber: number?): nil"
    ],
    "PluginToolbar": [
        "function CreateButton(self, id: string, toolTip: string, iconAsset: string, text: string?): PluginToolbarButton",
    ],
    "WorldRoot": [
        "function Raycast(self, origin: Vector3, direction: Vector3, raycastParams: RaycastParams?): RaycastResult?",
        "function ArePartsTouchingOthers(self, partList: { BasePart }, overlapIgnored: number?): boolean",
        "function BulkMoveTo(self, partList: { BasePart }, cframeList: { CFrame }, eventMode: EnumBulkMoveMode?): nil",
        "function GetPartBoundsInBox(self, cframe: CFrame, size: Vector3, overlapParams: OverlapParams?): { BasePart }",
        "function GetPartBoundsInRadius(self, position: Vector3, radius: number, overlapParams: OverlapParams?): { BasePart }",
        "function GetPartsInPart(self, part: BasePart, overlapParams: OverlapParams?): { BasePart }",
    ],
    "HttpService": [
        "function RequestAsync(self, options: HttpRequestOptions): HttpResponseData",
    ],
    "HumanoidDescription": [
        "function GetAccessories(self, includeRigidAccessories: boolean): { HumanoidDescriptionAccessory }",
        "function SetAccessories(self, accessories: { HumanoidDescriptionAccessory }, includeRigidAccessories: boolean): ()",
        "function GetEmotes(self): { [string]: { number } }",
        "function SetEmotes(self, emotes: { [string]: { number } }): ()",
        "function GetEquippedEmotes(self): { { Slot: number, Name: string } }",
        "function SetEquippedEmotes(self, equippedEmotes: { string } | { Slot: number, Name: string }): ()",
    ],
    "TeleportService": [
        "function GetLocalPlayerTeleportData(self): TeleportData?",
        "function GetPlayerPlaceInstanceAsync(self, userId: number): (boolean, string, number, string)",
        "function Teleport(self, placeId: number, player: Player?, teleportData: TeleportData?, customLoadingScreen: GuiObject?)",
        "function TeleportAsync(self, placeId: number, players: { Player }, teleportOptions: TeleportOptions?): TeleportAsyncResult",
        "function TeleportPartyAsync(self, placeId: number, players: { Player }, teleportData: TeleportData?, customLoadingScreen: GuiObject?): string",
        "function TeleportToPlaceInstance(self, placeId: number, instanceId: string, player: Player?, spawnName: string?, teleportData: TeleportData?, customLoadingScreen: GuiObject?)",
        "function TeleportToPrivateServer(self, placeId: number, reservedServerAccessCode: string, players: { Player }, spawnName: string?, teleportData: TeleportData?, customLoadingScreen: GuiObject?): nil",
        "function TeleportToSpawnByName(self, placeId: number, spawnName: string, player: Player?, teleportData: TeleportData?, customLoadingScreen: GuiObject?)",
        "function ReserveServer(self, placeId: number): (string, string)",
        "LocalPlayerArrivedFromTeleport: RBXScriptSignal<Player, any>",
        "TeleportInitFailed: RBXScriptSignal<Player, EnumTeleportResult, string, number, TeleportOptions>",
    ],
    "TeleportOptions": [
        "function GetTeleportData(self): TeleportData?",
        "function SetTeleportData(self, teleportData: TeleportData)",
    ],
    "UserService": [
        "function GetUserInfosByUserIdsAsync(self, userIds: { number }): { { Id: number, Username: string, DisplayName: string } }"
    ],
    "Studio": ["Theme: StudioTheme"],
    "BasePlayerGui": [
        "function GetGuiObjectsAtPosition(self, x: number, y: number): { GuiObject }",
        "function GetGuiObjectsInCircle(self, position: Vector2, radius: number): { GuiObject }",
    ],
    "Path": [
        "function GetWaypoints(self): { PathWaypoint }",
    ],
    "CollectionService": [
        "function GetAllTags(self): { string }",
        "function GetTags(self, instance: Instance): { string }",
        "function GetInstanceAddedSignal(self, tag: string): RBXScriptSignal<Instance>",
        "function GetInstanceRemovedSignal(self, tag: string): RBXScriptSignal<Instance>",
    ],
    "UserInputService": [
        "function GetConnectedGamepads(self): { EnumUserInputType }",
        "function GetGamepadState(self, gamepadNum: EnumUserInputType): { InputObject }",
        "function GetKeysPressed(self): { InputObject }",
        "function GetMouseButtonsPressed(self): { InputObject }",
        "function GetNavigationGamepads(self): { EnumUserInputType }",
        "function GetSupportedGamepadKeyCodes(self, gamepadNum: EnumUserInputType): { EnumKeyCode }",
    ],
    "Humanoid": [
        "RootPart: BasePart?",
        "SeatPart: Seat | VehicleSeat | nil",
        "WalkToPart: BasePart?",
        "function GetAccessories(self): { Accessory }",
    ],
    "Player": [
        "Character: Model?",
        "Chatted: RBXScriptSignal<string, Player?>",
        "ReplicationFocus: Instance?",
        "function GetJoinData(self): { LaunchData: string?, Members: {number}?, SourceGameId: number?, SourcePlaceId: number?, TeleportData: TeleportData? }",
    ],
    "InstanceAdornment": ["Adornee: Instance?"],
    "BasePart": [
        "function GetConnectedParts(self, recursive: boolean?): { BasePart }",
        "function GetJoints(self): { BasePart }",
        "function GetNetworkOwner(self): Player?",
        "function GetTouchingParts(self): { BasePart }",
        "function SubtractAsync(self, parts: { BasePart }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): UnionOperation",
        "function UnionAsync(self, parts: { BasePart }, collisionfidelity: EnumCollisionFidelity?, renderFidelity: EnumRenderFidelity?): UnionOperation",
    ],
    "Team": ["function GetPlayers(self): { Player }"],
    "Teams": [
        "function GetTeams(self): { Team }",
    ],
    "Camera": [
        "CameraSubject: Humanoid | BasePart | nil",
        "function GetPartsObscuringTarget(self, castPoints: { Vector3 }, ignoreList: { Instance }): { BasePart }",
    ],
    "RunService": [
        "function BindToRenderStep(self, name: string, priority: number, func: ((delta: number) -> ())): ()",
    ],
    "GuiService": ["SelectedObject: GuiObject?"],
    "TextChatService": [
        "ChatWindowConfiguration: ChatWindowConfiguration",
        "ChatInputBarConfiguration: ChatInputBarConfiguration",
        "BubbleChatConfiguration: BubbleChatConfiguration",
        "ChannelTabsConfiguration: ChannelTabsConfiguration",
    ],
    "GlobalDataStore": [
        # GetAsync we received from upstream didn't have a second return value of DataStoreKeyInfo
        "function GetAsync(self, key: string, options: DataStoreGetOptions?): (any, DataStoreKeyInfo)",
        # IncrementAsync didn't have a second return value of DataStoreKeyInfo, and the first return value is always a number
        "function IncrementAsync(self, key: string, delta: number?, userIds: { number }?, options: DataStoreIncrementOptions?): (number, DataStoreKeyInfo)",
        # RemoveAsync didn't have a second return value of DataStoreKeyInfo
        "function RemoveAsync(self, key: string): (any, DataStoreKeyInfo)",
        # SetAsync returns the version as a string, upstream says it's any
        "function SetAsync(self, key: string, value: any, userIds: { number }?, options: DataStoreSetOptions?): string",
        # UpdateAsync didn't have a second return value of DataStoreKeyInfo
        # and the second parameter passed to the transform function is always a DataStoreKeyInfo
        # and the return of the transform function was incorrect
        "function UpdateAsync(self, key: string, transformFunction: ((any, DataStoreKeyInfo) -> (any, { number }?, {}?))): (any, DataStoreKeyInfo)",
    ],
    # Trying to set the value in a OrderedDataStore to anything other than a number will error,
    # So we override the method's types to use numbers instead of any
    "OrderedDataStore": [
        "function GetAsync(self, key: string, options: DataStoreGetOptions?): (number?, DataStoreKeyInfo)",
        "function GetSortedAsync(self, ascending: boolean, pageSize: number, minValue: number?, maxValue: number?): DataStorePages",
        "function RemoveAsync(self, key: string): (number?, DataStoreKeyInfo)",
        "function SetAsync(self, key: string, value: number, userIds: { number }?, options: DataStoreSetOptions?): string",
        "function UpdateAsync(self, key: string, transformFunction: ((number?, DataStoreKeyInfo) -> (number, { number }?, {}?))): (number?, DataStoreKeyInfo)",
    ],
    # The Adornee property is optional
    "Highlight": ["Adornee: Instance?"],
    "PartAdornment": ["Adornee: BasePart?"],
    "JointInstance": [
        "Part0: BasePart?",
        "Part1: BasePart?",
    ],
    "ObjectValue": [
        "Value: Instance?",
        "Changed: RBXScriptSignal<Instance?>",
    ],
    "Actor": [
        "function SendMessage(self, topic: string, ...: any): ()",
    ],
    "Seat": [
        "Occupant: Humanoid?",
    ],
    "VehicleSeat": [
        "Occupant: Humanoid?",
    ],
    "Beam": [
        "Attachment0: Attachment?",
        "Attachment1: Attachment?",
    ],
    "Trail": [
        "Attachment0: Attachment?",
        "Attachment1: Attachment?",
    ],
    "Constraint": [
        "Attachment0: Attachment?",
        "Attachment1: Attachment?",
    ],
    "PathfindingLink": [
        "Attachment0: Attachment?",
        "Attachment1: Attachment?",
    ],
    "ControllerPartSensor": [
        "SensedPart: BasePart?",
    ],
    "Sound": [
        "SoundGroup: SoundGroup?",
    ],
    "StudioService": [
        "function GizmoRaycast(self, origin: Vector3, direction: Vector3, raycastParams: RaycastParams?): RaycastResult<Attachment | Constraint | NoCollisionConstraint | WeldConstraint>?"
    ],
    "ControllerManager": [
        "ActiveController: ControllerBase?",
        "ClimbSensor: ControllerSensor?",
        "GroundSensor: ControllerSensor?",
        "RootPart: BasePart?",
    ],
    "CaptureService": [
        "function StartVideoCaptureAsync(self, onCaptureReady: (capture: VideoCapture) -> (), params: CaptureParams): EnumVideoCaptureStartedResult",
        "function TakeCapture(self, onCaptureReady: (capture: Capture) -> (), params: CaptureParams): ()",
    ],
    "ModerationService": [
        "function BindReviewableContentEventProcessor(self, priority: number, callback: (event: ReviewableContentEvent) -> ()): RBXScriptConnection"
    ],
    "VideoSampler": [
        "function GetSamplesAtTimesAsync(self, times: { number }): { VideoSample }"
    ],
    "AvatarCreationService": [
        "function AutoSetupAvatarAsync(self, player: Player, model: Model, progressCallback: (progressInfo: { Progress: number }) -> ()?): string",
        "function AutoSetupAvatarNewAsync(self, player: Player, autoSetupParams: AutoSetupParams, progressCallback: (progressInfo: { Progress: number }) -> ()?): string"
    ]
}

# Hardcoded types
# These will go before anything else, and are useful to define for other tools
START_BASE = """
type ContentId = string
type ProtectedString = string
type BinaryString = string
type QDir = string
type QFont = string
type FloatCurveKey = any
type RotationCurveKey = any
type Secret = any
type Path2DControlPoint = any
type UniqueId = any
type SecurityCapabilities = any
type TeleportData = boolean | buffer | number | string | {[number]: TeleportData} | {[string]: TeleportData}
type SystemAddress = any
type AdReward = any

declare class Enum
    function GetEnumItems(self): { any }
    function FromValue(self,Number: number): any
    function FromName(self,Name: string): any
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
    getmemorycategory: () -> string,
    setmemorycategory: (tag: string) -> (),
    resetmemorycategory: () -> (),
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

declare function warn<T...>(...: T...)

@[deprecated { use = "task.spawn" }]
declare function spawn(callback: (dt: number, gt: number) -> ())
"""

POST_DATATYPES_BASE = """
declare class SharedTable
  [string | number]: any
  function __iter(self): (any, number) -> (number, any)
end

export type OpenCloudModel = any
export type ClipEvaluator = any

export type RBXScriptSignal<T... = ...any> = {
    Wait: (self: RBXScriptSignal<T...>) -> T...,
    Connect: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    ConnectParallel: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    Once: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
}

type HttpRequestOptions = {
    Url: string,
    Method: "GET" | "HEAD" | "POST" | "PUT" | "DELETE" | "CONNECT" | "OPTIONS" | "TRACE" | "PATCH" | nil,
    Headers: { [string]: string | Secret }?,
    Body: string?,
    Compress: EnumHttpCompression
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

declare class ValueCurveKey
    Interpolation: EnumKeyInterpolationMode
    Time: number
    Value: any
    RightTangent: number
    LeftTangent: number
end

declare ValueCurveKey: {
    new: (time: number, value: any, interpolation: EnumKeyInterpolationMode) -> ValueCurveKey
}
"""

# More hardcoded types, but go at the end of the file
# Useful if they rely on previously defined types
END_BASE = """
export type RaycastResult<T = BasePart> = {
    Instance: T,
    Position: Vector3,
    Normal: Vector3,
    Material: EnumMaterial,
    Distance: number,
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
    new: (t: { [any]: any }?) -> SharedTable,
    clear: (st: SharedTable) -> (),
    clone: (st: SharedTable, deep: boolean?) -> SharedTable,
    cloneAndFreeze: (st: SharedTable, deep: boolean?) -> SharedTable,
    increment: (st: SharedTable, key: string | number, delta: number) -> number,
    isFrozen: (st: SharedTable) -> boolean,
    size: (st: SharedTable) -> number,
    update: (st: SharedTable, key: string | number, f: (any) -> any) -> (),
}

declare function settings(): GlobalSettings
declare function UserSettings(): UserSettings

@[deprecated {use = "plugin"}]
declare function PluginManager(): PluginManager

@[deprecated {use = 'game:GetService("DebuggerManager")'}]
declare function DebuggerManager(): DebuggerManager
"""

CLASSES = {}  # All loaded classes from the API Dump, including corrections
SERVICES: List[str] = []  # All available services name
CREATABLE: List[str] = []  # All creatable instances
BRICK_COLORS: Set[str] = set()

# Type Hints

CorrectionsValueType = TypedDict(
    "CorrectionsValueType",
    {
        "Name": str,
        "Category": None,
        "Default": Optional[str],
        "Generic": Optional[str],
        "Declared": Optional[str],
        "Tuple": Optional["CorrectionsValueType"],
        "Union": Optional[List["CorrectionsValueType"]],
    },
)

CorrectionsParameter = TypedDict(
    "CorrectionsParameter",
    {
        "Name": str,
        "Type": CorrectionsValueType,
    },
)

CorrectionsMember = TypedDict(
    "CorrectionsMember",
    {
        "Name": str,
        "ValueType": Optional[CorrectionsValueType],
        "Parameters": Optional[List[CorrectionsParameter]],
        "ReturnType": Optional[CorrectionsValueType],
        "TupleReturns": Optional[List[CorrectionsValueType]],
    },
)

CorrectionsClass = TypedDict(
    "CorrectionsClass", {"Name": str, "Members": List[CorrectionsMember]}
)

CorrectionsDump = TypedDict("CorrectionsDump", {"Classes": List[CorrectionsClass]})

ApiSecurityLevel = Union[
    Literal["None"],
    Literal["LocalUserSecurity"],
    Literal["PluginSecurity"],
    Literal["WritePlayerSecurity"],
    Literal["RobloxScriptSecurity"],
    Literal["RobloxSecurity"],
    Literal["NotAccessibleSecurity"],
]

ApiDeprecatedInfo = TypedDict(
    "ApiDeprecatedInfo",
    {
        "PreferredDescriptorName": str,
        "ThreadSafety": str,
    }
)

ApiTags = Optional[List[str | ApiDeprecatedInfo]]

ApiValueType = TypedDict(
    "ApiValueType",
    {
        "Name": str,
        "Category": Union[
            Literal["Primitive"],
            Literal["Class"],
            Literal["DataType"],
            Literal["Enum"],
            Literal["Group"],  # Name = "Tuple"
        ],
    },
)

ApiParameter = TypedDict(
    "ApiParameter",
    {
        "Name": str,
        "Type": ApiValueType,
        "Default": Optional[str],
    },
)

ApiPropertySecurityLevel = TypedDict(
    "ApiPropertySecurityLevel", 
    {
        "Read": ApiSecurityLevel,
        "Write": ApiSecurityLevel
    }
)

ApiProperty = TypedDict(
    "ApiProperty",
    {
        "Name": str,
        "MemberType": Literal["Property"],
        "Description": Optional[str],
        "Tags": ApiTags,
        "Category": str,
        "ValueType": ApiValueType,
        "Security": ApiPropertySecurityLevel,
    },
)

ApiFunction = TypedDict(
    "ApiFunction",
    {
        "Name": str,
        "MemberType": Literal["Function"],
        "Description": Optional[str],
        "Parameters": List[ApiParameter],
        "ReturnType": Optional[Union[ApiValueType, List[ApiValueType]]],
        "TupleReturns": Optional[List[CorrectionsValueType]],
        "Security": ApiSecurityLevel,
        "Tags": ApiTags,
    },
)

ApiEvent = TypedDict(
    "ApiEvent",
    {
        "Name": str,
        "MemberType": Literal["Event"],
        "Description": Optional[str],
        "Parameters": List[ApiParameter],
        "Security": ApiSecurityLevel,
        "Tags": ApiTags,
    },
)

ApiCallback = TypedDict(
    "ApiCallback",
    {
        "Name": str,
        "MemberType": Literal["Callback"],
        "Description": Optional[str],
        "Parameters": List[ApiParameter],
        "ReturnType": Optional[Union[ApiValueType, List[ApiValueType]]],
        "TupleReturns": Optional[List[CorrectionsValueType]],
        "Tags": ApiTags,
        "Security": ApiSecurityLevel,
    },
)

ApiMember = Union[ApiProperty, ApiFunction, ApiEvent, ApiCallback]

ApiClass = TypedDict(
    "ApiClass",
    {
        "Name": str,
        "Description": Optional[str],
        "MemoryCategory": str,  # TODO: stricter type?
        "Superclass": str,
        "Members": List[ApiMember],
        "Tags": ApiTags,
    },
)

ApiEnumItem = TypedDict(
    "ApiEnumItem",
    {
        "Name": str,
        "Value": int,
        "Description": Optional[str],
    },
)

ApiEnum = TypedDict(
    "ApiEnum",
    {
        "Name": str,
        "Description": Optional[str],
        "Items": List[ApiEnumItem],
    },
)

ApiDump = TypedDict(
    "ApiDump",
    {
        "Version": int,
        "Classes": List[ApiClass],
        "Enums": List[ApiEnum],
    },
)

DataType = TypedDict("DataType", {"Name": str, "Members": List[ApiMember]})

DataTypesConstructor = TypedDict(
    "DataTypesConstructor", {"Name": str, "Members": List[ApiMember]}
)

DataTypesDump = TypedDict(
    "DataTypesDump",
    {"DataTypes": List[DataType], "Constructors": List[DataTypesConstructor]},
)

chosenSecurityLevel = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SECURITY_LEVEL
assert (
        chosenSecurityLevel in SECURITY_LEVELS
), f"Unknown security level: {chosenSecurityLevel}"

# Cache for looking up members by name when resolving deprecations
classesWithMemberName: dict[str, List[ApiClass]] = {}

# Keep track of declared Luau types as a failsafe if we need to declare them.
# This needs to be kept in-sync with any Luau type declarations
# added in EXTRA_MEMBERS

declaredLuauTypes: Set[str] = {
    "ReviewableContentEvent",
    "AutoSetupParams",
    "CaptureParams",
    "VideoSample",
}

def isIdentifier(name: str):
    return re.match(r"^[a-zA-Z_]+[a-zA-Z_0-9]*$", name)  # TODO: 'function'

def escapeName(name: str):
    """Escape a name string to be property-compatible"""
    if name == "function":
        return "func"
    if not isIdentifier(name):
        escape_quotes = name.replace('"', '\\"')
        return f'["{escape_quotes}"]'
    return name


def resolveType(type: Union[ApiValueType, CorrectionsValueType]) -> str:
    if "Generic" in type and type["Generic"] is not None:
        name = type["Generic"]
        if name.startswith("Enum."):
            name = "Enum" + name[5:]
        return "{ " + name + " }"

    if "Union" in type and type["Union"] is not None:
        unions = type["Union"]
        parts = [resolveType(part) for part in unions]
        return " | ".join(parts)

    if "Tuple" in type and type["Tuple"] is not None:
        subtype = resolveType(type["Tuple"])
        return f"...({subtype})"
    if "Variadic" in type and type["Variadic"] is not None:
        subtype = resolveType(type["Variadic"])
        return f"...{subtype}"

    if "Declared" in type and type["Declared"] is not None:
        type["Name"] = type["Declared"]

    name, category = (
        type["Name"],
        type["Category"] if "Category" in type else "Primitive",
    )

    if name[-1] == "?":
        return resolveType({"Name": name[:-1], "Category": category or "Primitive"}) + "?"

    if category == "Enum":
        return "Enum" + name
    else:
        return TYPE_INDEX[name] if name in TYPE_INDEX else name


def resolveParameter(param: ApiParameter):
    paramType = resolveType(param["Type"])
    isOptional = paramType[-1] == "?"
    isVariadic = paramType.startswith("...")
    if isVariadic:
        actualType = paramType[3:]
        if "Variadic" in param["Type"] and param["Type"]["Variadic"] is not None:
            return f"...{actualType}"

        return f"...: {actualType}"
    return f"{escapeName(param['Name'])}: {paramType}{'?' if 'Default' in param and not isOptional else ''}"


def resolveParameterList(params: List[ApiParameter]):
    return ", ".join(map(resolveParameter, params))


def resolveReturnType(member: Union[ApiFunction, ApiCallback]) -> str:
    if "TupleReturns" in member and member["TupleReturns"] is not None:
        types = [resolveType(ret) for ret in member["TupleReturns"]]
        return "(" + ", ".join(types) + ")"
    elif isinstance(member["ReturnType"], list):
        types = [resolveType(ret) for ret in member["ReturnType"]]
        return "(" + ", ".join(types) + ")"
    elif member["ReturnType"] is not None:
        return resolveType(member["ReturnType"])
    
    return "nil"

def resolveDeprecation(member: ApiMember, klass: ApiClass | DataType) -> str:
    tags: Optional[List[Union[str, ApiDeprecatedInfo]]] = None
    
    if "Tags" in member:
        tags = member["Tags"]
    
    result = ""

    if tags is not None:
        for tag in tags:
            if tag == "Deprecated":
                result = f"@deprecated\n\t\t"
            elif not isinstance(tag, str) and "PreferredDescriptorName" in tag:
                preferred = tag["PreferredDescriptorName"]
                matchingClasses = classesWithMemberName.get(preferred)

                if matchingClasses is None:
                    # Check if the class itself contains the preferred member
                    # This happens if the preferred member is deprecated too.
                    if preferred in [member["Name"] for member in klass["Members"]]:
                        matchingClasses = [klass]

                if matchingClasses is not None:
                    bestClass: Optional[ApiClass | DataType] = None

                    if klass in matchingClasses:
                        bestClass = klass
                    else:
                        # TODO: Better selection logic
                        bestClass = matchingClasses[0]

                    if bestClass is not None:
                        bestMember: Optional[ApiMember] = None

                        for member in bestClass["Members"]:
                            if member["Name"] == preferred:
                                bestMember = member
                                break
                        
                        if bestMember is not None:
                            # Use the classname and member name, we found a different class to point to!
                            result = f"@[deprecated {{use = \"{bestClass['Name']}{':' if bestMember['MemberType'] == 'Function' else '.'}{preferred}\"}}]\n\t\t"
                            break

                result = f"@[deprecated {{use = \"{preferred}\"}}]\n\t\t"
                break

    return result

def classIgnoredMembers(klassName: str):
    ignoredMembers = []

    if klassName in EXTRA_MEMBERS:
        for member in EXTRA_MEMBERS[klassName]:
            if member.startswith("function "):
                functionName = member[len("function "):]
                functionName = functionName[: functionName.find("(")]
                ignoredMembers.append(functionName)
            else:
                colon = member.find(":")
                assert colon != -1, member
                ignoredMembers.append(member[:colon])

    return ignoredMembers


def filterMember(klassName: str, member: ApiMember):
    if not INCLUDE_DEPRECATED_MEMBERS and (
        "Tags" in member and
        member["Tags"] is not None
        and "Deprecated" in member["Tags"]
        and member["MemberType"] != "Function"
    ):
        return False
    
    if ("Tags" in member and
        member["Tags"] is not None
        and "NotScriptable" in member["Tags"]):
        return False
    
    if member["Name"] in classIgnoredMembers(klassName):
        return False
    

    if "Security" in member:
        if isinstance(member["Security"], str):
            if SECURITY_LEVELS.index(member["Security"]) > SECURITY_LEVELS.index(
                    chosenSecurityLevel
            ):
                return False
        else:
            if min(
                    SECURITY_LEVELS.index(member["Security"]["Read"]),
                    SECURITY_LEVELS.index(member["Security"]["Write"]),
            ) > SECURITY_LEVELS.index(chosenSecurityLevel):
                return False

    return True


def declareClass(klass: Union[ApiClass, DataType]) -> str:
    if klass["Name"] in IGNORED_INSTANCES:
        return ""

    out = "declare class " + klass["Name"]
    if "Superclass" in klass and klass["Superclass"] != "<<<ROOT>>>":
        out += " extends " + klass["Superclass"]
    out += "\n"

    def declareMember(member: ApiMember):
        if member["MemberType"] == "Property":
            return (
                f"\t{escapeName(member['Name'])}: {resolveType(member['ValueType'])}\n"
            )
        elif member["MemberType"] == "Function":
            return f"\t{resolveDeprecation(member, klass)}function {escapeName(member['Name'])}(self{', ' if len(member['Parameters']) > 0 else ''}{resolveParameterList(member['Parameters'])}): {resolveReturnType(member)}\n"
        elif member["MemberType"] == "Event":
            parameters = ", ".join(
                map(lambda x: resolveType(x["Type"]), member["Parameters"])
            )
            return f"\t{escapeName(member['Name'])}: RBXScriptSignal<{parameters}>\n"
        elif member["MemberType"] == "Callback":
            return f"\t{escapeName(member['Name'])}: ({resolveParameterList(member['Parameters'])}) -> {resolveReturnType(member)}\n"
        else:
            assert False, "Unhandled member type: " + member["MemberType"]

    memberDefinitions = [
        declareMember(m) for m in klass["Members"] if filterMember(klass["Name"], m)
    ]

    if klass["Name"] in EXTRA_MEMBERS:
        memberDefinitions += [
            f"\t{member}\n" for member in EXTRA_MEMBERS[klass["Name"]]
        ]

    out += "".join(sorted(memberDefinitions))
    out += "end"

    return out


def printEnums(dump: ApiDump):
    enums: dict[str, List[str]] = {}
    for enum in dump["Enums"]:
        enums[enum["Name"]] = []
        for item in enum["Items"]:
            enums[enum["Name"]].append(item["Name"])

    # Declare each enum individually
    out = ""
    for enum, items in enums.items():
        # Declare an atom for the enum
        out += f"declare class Enum{enum} extends EnumItem end\n"
        out += f"declare class Enum{enum}_INTERNAL extends Enum\n"
        items.sort()
        for item in items:
            out += f"\t{escapeName(item)}: Enum{enum}\n"
        out += f"\tfunction GetEnumItems(self): {{ Enum{enum} }}\n"
        out += f"\tfunction FromName(self, Name: string): Enum{enum}?\n"
        out += f"\tfunction FromValue(self, Value: number): Enum{enum}?\n"
        out += "end\n"
    print(out)
    print()

    # Declare enums as a whole
    out = "type ENUM_LIST = {\n"
    for enum in enums:
        out += f"\t{enum}: Enum{enum}_INTERNAL,\n"
    out += "} & { GetEnums: (self: ENUM_LIST) -> { Enum } }\n"
    out += "declare Enum: ENUM_LIST"
    print(out)
    print()


def printClasses(dump: ApiDump):
    for klass in dump["Classes"]:
        if klass["Name"] in IGNORED_INSTANCES:
            continue

        print(declareClass(klass))
        print()


def printDataTypes(types: List[DataType], dump: ApiDump):
    for klass in types:
        print(declareClass(klass))
        print()


def printDataTypeConstructors(types: DataTypesDump):
    for klass in types["Constructors"]:
        if klass["Name"] in IGNORED_INSTANCES:
            continue
        name = klass["Name"]
        members = klass["Members"]

        isBrickColorNew = False

        # Handle overloadable functions
        functions: defaultdict[str, List[ApiFunction]] = defaultdict(list)
        for member in members:
            if member["MemberType"] == "Function":
                if (
                        name == "BrickColor"
                        and member["Name"] == "new"
                        and len(member["Parameters"]) == 1
                        and member["Parameters"][0]["Type"]["Name"] == "string"
                ):
                    isBrickColorNew = True
                    continue
                functions[member["Name"]].append(member)

        out = "declare " + name + ": {\n"
        for member in members:
            if member["MemberType"] == "Property":
                out += f"\t{escapeName(member['Name'])}: {resolveType(member['ValueType'])},\n"
            elif member["MemberType"] == "Function":
                pass
            elif member["MemberType"] == "Event":
                out += f"\t{escapeName(member['Name'])}: RBXScriptSignal,\n"  # TODO: type this

        # Special case string BrickColor new
        if isBrickColorNew:
            colors = " | ".join(map(lambda c: f'"{c}"', sorted(BRICK_COLORS)))

            functions["new"].append(
                {
                    "MemberType": "Function",
                    "Description": None,
                    "TupleReturns": None,
                    "Security": "None",
                    "Tags": None,
                    "Parameters": [{"Name": "name", "Type": {"Category": "Primitive", "Name": colors}, "Default": None}],
                    "ReturnType": {"Name": "BrickColor", "Category": "DataType"},
                    "Name": "new",
                }
            )

        for function, overloads in functions.items():
            overloads = map(
                lambda member: f"(({resolveParameterList(member['Parameters'])}) -> {resolveReturnType(member)})",
                overloads,
            )
            out += f"\t{escapeName(function)}: {' & '.join(overloads)},\n"

        out += "}"
        print(out)
        print()


def applyCorrections(dump: ApiDump, corrections: CorrectionsDump):
    for klass in corrections["Classes"]:
        for otherClass in dump["Classes"]:
            if otherClass["Name"] == klass["Name"]:
                for member in klass["Members"]:
                    for otherMember in otherClass["Members"]:
                        if otherMember["Name"] == member["Name"]:
                            if "TupleReturns" in member:
                                if "ReturnType" in otherMember:
                                    otherMember["ReturnType"] = None
                                otherMember["TupleReturns"] = member["TupleReturns"]
                            elif "ReturnType" in member:
                                otherMember["ReturnType"]["Name"] = (
                                    member["ReturnType"]["Name"]
                                    if "Name" in member["ReturnType"]
                                    else otherMember["ReturnType"]["Name"]
                                )
                                if "Generic" in member["ReturnType"]:
                                    otherMember["ReturnType"]["Generic"] = member[
                                        "ReturnType"
                                    ]["Generic"]
                            elif "ValueType" in member:
                                otherMember["ValueType"]["Name"] = (
                                    member["ValueType"]["Name"]
                                    if "Name" in member["ValueType"]
                                    else otherMember["ValueType"]["Name"]
                                )
                                if "Generic" in member["ValueType"]:
                                    otherMember["ValueType"]["Generic"] = member[
                                        "ValueType"
                                    ]["Generic"]

                            if (
                                    "Parameters" in member
                                    and member["Parameters"] is not None
                            ):
                                for param in member["Parameters"]:
                                    for otherParam in otherMember["Parameters"]:
                                        if otherParam["Name"] == param["Name"]:
                                            if "Type" in param:
                                                otherParam["Type"]["Name"] = (
                                                    param["Type"]["Name"]
                                                    if "Name" in param["Type"]
                                                    else otherParam["Type"]["Name"]
                                                )
                                                if "Generic" in param["Type"]:
                                                    otherParam["Type"]["Generic"] = (
                                                        param["Type"]["Generic"]
                                                    )
                                            if "Default" in param:
                                                otherParam["Default"] = param["Default"]

                            break
                break

def loadMembersIntoStructures(klass: ApiClass):
    for member in klass["Members"]:
        name = member["Name"]
        tags = member["Tags"] if "Tags" in member else None

        if (tags is not None and "Deprecated" in tags):
            continue

        if name not in classesWithMemberName:
            classesWithMemberName[name] = [klass]
        else:
            classesWithMemberName[name].append(klass)
            

def loadClassesIntoStructures(dump: ApiDump):
    for klass in dump["Classes"]:
        if klass["Name"] in IGNORED_INSTANCES:
            continue

        isCreatable = True
        if "Tags" in klass and klass["Tags"] is not None:
            if "Service" in klass["Tags"]:
                SERVICES.append(klass["Name"])
            if "NotCreatable" in klass["Tags"]:
                isCreatable = False
        if isCreatable:
            CREATABLE.append(klass["Name"])

        CLASSES[klass["Name"]] = klass
        loadMembersIntoStructures(klass)

def registerDeclaredInType(type: CorrectionsValueType | None):
    if type is not None:
        if "Declared" in type and type["Declared"] is not None:
            if type["Declared"] not in declaredLuauTypes:
                declaredType = type["Declared"]

                if declaredType.endswith("?"):
                    declaredType = declaredType[:-1]

                if re.match("^[A-z0-9_]+$", declaredType):
                    declaredLuauTypes.add(declaredType)

def registerDeclared(dump: CorrectionsDump):
    for klass in dump["Classes"]:
        for member in klass["Members"]:
            if "TupleReturns" in member and member["TupleReturns"] is not None:
                for ret in member["TupleReturns"]:
                    registerDeclaredInType(ret)
            
            if "ReturnType" in member:
                if isinstance(member["ReturnType"], list):
                    for ret in member["ReturnType"]:
                        registerDeclaredInType(ret)
                else:
                    ret = member["ReturnType"]
                    registerDeclaredInType(ret)

            if "ValueType" in member:
                value = member["ValueType"]
                registerDeclaredInType(value)
            
            if "Parameters" in member and member["Parameters"] is not None:
                for param in member["Parameters"]:
                    if "Type" in param:
                        paramType = param["Type"]
                        registerDeclaredInType(paramType)

def processBrickColors(colors):
    for color in colors["BrickColors"]:
        BRICK_COLORS.add(color["Name"])


def printJsonPrologue():
    data = {"CREATABLE_INSTANCES": CREATABLE, "SERVICES": SERVICES}
    print("--#METADATA#" + json.dumps(data, indent=None))
    print()

def printLuauTypes():
    luauTypes: str = requests.get(LUAU_TYPES_URL).text

    # Split luauTypes into lines
    luauLines = luauTypes.splitlines()

    while not luauLines[0].startswith("-- SECTION BEGIN:"):
        luauLines.pop(0)
    
    # Begin capturing sections from SECTION BEGIN to SECTION END
    luauSections: dict[str, list[str]] = dict()
    sectionNames: list[str] = []
    currentSection = None

    for line in luauLines:
        if line.startswith("-- SECTION BEGIN:"):
            currentSection = []
        elif line.startswith("-- SECTION END:"):
            sectionName = line[16:]

            if currentSection is not None:
                luauSections[sectionName] = currentSection
                currentSection = None
            
            sectionNames.append(sectionName)
        elif currentSection is not None:
            if line in LUAU_SNIPPET_PATCHES.keys():
                line = LUAU_SNIPPET_PATCHES[line]
            
            line = line.replace("Enum.", "Enum")
            currentSection.append(line)

    # Reconstruct luauTypes
    writtenLines: set[str] = set()
    luauTypes = ""

    for sectionName in sectionNames:
        if sectionName not in DELETED_LUAU_SECTIONS:
            sectionLines = luauSections[sectionName]

            for line in sectionLines:
                writtenLines.add(line)
            
            luauTypes += "\n".join(sectionLines) + "\n"
    
    # Fail-safe: Append any patches that were not written
    for patch in LUAU_SNIPPET_PATCHES.values():
        if patch not in writtenLines:
            luauTypes += patch + "\n"

    # Fail-safe: Declare any missing types that were marked as declared
    for declaredType in declaredLuauTypes:
        declaration = f"type {declaredType} = any"
        found = False

        for line in writtenLines:
            if line.find(declaredType) != -1:
                found = True
                break

        if not found:
            luauTypes += declaration + "\n"
    
    print(luauTypes)

# Load BrickColors
brickColors = json.loads(requests.get(BRICK_COLORS_URL).text)
processBrickColors(brickColors)

# Print global types
dataTypes: DataTypesDump = json.load(DATA_TYPES)
dump: ApiDump = json.loads(requests.get(API_DUMP_URL).text)

# Load services and creatable instances
loadClassesIntoStructures(dump)

# Apply any corrections on the dump
corrections: CorrectionsDump = json.load(CORRECTIONS)
applyCorrections(dump, corrections)
registerDeclared(corrections)

printJsonPrologue()
print(START_BASE)
printEnums(dump)
printDataTypes(sorted(dataTypes["DataTypes"], key=lambda klass: klass["Name"]), dump)
print(POST_DATATYPES_BASE)
printLuauTypes()
printClasses(dump)
printDataTypeConstructors(dataTypes)
print(END_BASE)
