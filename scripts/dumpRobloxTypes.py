# Script to pull in API Dump and export it into a definition file
# Based off https://gist.github.com/HawDevelopment/97f2411149e24d8e7a712016114d55ff
import re
from typing import List, Optional, Set
from collections import defaultdict
import requests
import json
import sys
import yaml
import os
from dataclasses import dataclass

# API Endpoints
DOCUMENTATION_DIRECTORY = "creator-docs/content/en-us/reference/engine"
BRICK_COLORS_URL = "https://gist.githubusercontent.com/Anaminus/49ac255a68e7a7bc3cdd72b602d5071f/raw/f1534dcae312dbfda716b7677f8ac338b565afc3/BrickColor.json"

INCLUDE_DEPRECATED_METHODS = False
DEFAULT_SECURITY_LEVEL = "RobloxScriptSecurity"
SECURITY_LEVELS = [
    "None",
    "LocalUserSecurity",
    "PluginSecurity",
    "RobloxScriptSecurity",
    "RobloxSecurity",
    "NotAccessibleSecurity",
]

# Classes which should still be kept even though they are marked deprecated: (mainly the bodymovers)
OVERRIDE_DEPRECATED_REMOVAL = [
    "BodyMover",
    "BodyAngularVelocity",
    "BodyForce",
    "BodyGyro",
    "BodyPosition",
    "BodyThrust",
    "BodyVelocity",
    # "RocketPropulsion",
]

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
    "Dictionary": "{ [any]: any }",
    "Map": "{ [any]: any }",
    "Array": "{ any }",
    "table": "{ any }",
    "CoordinateFrame": "CFrame",
    "OptionalCoordinateFrame": "CFrame?",
}

IGNORED_INSTANCES: List[str] = [
    "RBXScriptSignal",  # Redefined using generics
    "BlockMesh",  # its superclass is marked as deprecated but it isn't, so its broken
    "Enum",  # redefined explicitly
    "EnumItem",  # redefined explicitly
    "GlobalSettings",  # redefined explicitly
]

# Methods / Properties ignored in classes. Commonly used to add corrections
IGNORED_MEMBERS = {
    "Instance": [
        "Parent",
        "FindFirstChild",
        "FindFirstChildOfClass",
        "FindFirstChildWhichIsA",
        "FindFirstAncestor",
        "FindFirstAncestorOfClass",
        "FindFirstAncestorWhichIsA",
        "FindFirstDescendant",
        "GetActor",
        "WaitForChild",
        "GetAttribute",
        "GetAttributes",
        "AncestryChanged",
        "GetAttributeChangedSignal",
        "GetPropertyChangedSignal",
    ],
    "Model": ["PrimaryPart"],
    "RemoteEvent": [
        "FireAllClients",
        "FireClient",
        "FireServer",
        "OnClientEvent",
        "OnServerEvent",
    ],
    "UnreliableRemoteEvent": [
        "FireAllClients",
        "FireClient",
        "FireServer",
        "OnClientEvent",
        "OnServerEvent",
    ],
    "RemoteFunction": [
        "InvokeClient",
        "InvokeServer",
        "OnClientInvoke",
        "OnServerInvoke",
    ],
    "BindableEvent": [
        "Fire",
        "Event",
    ],
    "BindableFunction": [
        "Invoke",
        "OnInvoke",
    ],
    "Players": [
        "PlayerChatted",
        "GetPlayerByUserId",
        "GetPlayerFromCharacter",
    ],
    "ContextActionService": ["BindAction", "BindActionAtPriority"],
    "Plugin": ["OpenScript"],
    "PluginToolbar": [
        "CreateButton",
    ],
    "WorldRoot": [
        "Raycast",
        "ArePartsTouchingOthers",
        "BulkMoveTo",
        "GetPartBoundsInBox",
        "GetPartBoundsInRadius",
        "GetPartsInPart",
    ],
    "HttpService": ["RequestAsync"],
    "HumanoidDescription": [
        "GetAccessories",
        "SetAccessories",
        "GetEmotes",
        "SetEmotes",
        "GetEquippedEmotes",
        "SetEquippedEmotes",
    ],
    "TeleportOptions": [
        "GetTeleportData",
        "SetTeleportData",
    ],
    "TeleportService": [
        "GetLocalPlayerTeleportData",
        "GetPlayerPlaceInstanceAsync",
        "Teleport",
        "TeleportAsync",
        "TeleportPartyAsync",
        "TeleportToPlaceInstance",
        "TeleportToPrivateServer",
        "TeleportToSpawnByName",
        "ReserveServer",
        "LocalPlayerArrivedFromTeleport",
        "TeleportInitFailed",
    ],
    "UserService": {"GetUserInfosByUserIdsAsync"},
    "Studio": {"Theme"},
    "BasePlayerGui": [
        "GetGuiObjectsAtPosition",
        "GetGuiObjectsInCircle",
    ],
    "Path": [
        "GetWaypoints",
    ],
    "CollectionService": [
        "GetAllTags",
        "GetTags",
        "GetInstanceAddedSignal",
        "GetInstanceRemovedSignal",
    ],
    "UserInputService": [
        "GetConnectedGamepads",
        "GetGamepadState",
        "GetKeysPressed",
        "GetMouseButtonsPressed",
        "GetNavigationGamepads",
        "GetSupportedGamepadKeyCodes",
    ],
    "Humanoid": [
        "RootPart",
        "SeatPart",
        "WalkToPart",
        "GetAccessories",
    ],
    "Player": [
        "Character",
        "Chatted",
        "GetJoinData",
    ],
    "InstanceAdornment": ["Adornee"],
    "BasePart": [
        "GetConnectedParts",
        "GetJoints",
        "GetNetworkOwner",
        "GetTouchingParts",
        "SubtractAsync",
        "UnionAsync",
    ],
    "Team": ["GetPlayers"],
    "Teams": ["GetTeams"],
    "Camera": [
        "CameraSubject",
        "GetPartsObscuringTarget",
    ],
    "RunService": [
        "BindToRenderStep",
    ],
    "GuiService": ["SelectedObject"],
    "GlobalDataStore": [
        "GetAsync",
        "IncrementAsync",
        "RemoveAsync",
        "SetAsync",
        "UpdateAsync",
    ],
    "OrderedDataStore": [
        "GetAsync",
        "GetSortedAsync",
        "RemoveAsync",
        "SetAsync",
        "UpdateAsync",
    ],
    "Highlight": ["Adornee"],
    "PartAdornment": ["Adornee"],
    "JointInstance": [
        "Part0",
        "Part1",
    ],
    "ObjectValue": [
        "Value",
        "Changed",
    ],
    "Actor": [
        "SendMessage",
    ],
    "Seat": [
        "Occupant",
    ],
    "VehicleSeat": [
        "Occupant",
    ],
    "Beam": [
        "Attachment0",
        "Attachment1",
    ],
    "Trail": [
        "Attachment0",
        "Attachment1",
    ],
    "Constraint": [
        "Attachment0",
        "Attachment1",
    ],
    "PathfindingLink": [
        "Attachment0",
        "Attachment1",
    ],
    "ControllerPartSensor": [
        "SensedPart",
    ],
}

# Extra members to add in to classes, commonly used to add in metamethods, and add corrections
EXTRA_MEMBERS = {
    "UserSettings": [
        "GameSettings: UserGameSettings",
        'function GetService(self, service: "UserGameSettings"): UserGameSettings',
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
        "function GetPropertyChangedSignal(self, property: string): RBXScriptSignal<>",
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
        "function Blockcast(self, cframe: CFrame, size: Vector3, direction: Vector3, params: RaycastParams?): RaycastResult?",
        "function Shapecast(self, part: BasePart, direction: Vector3, params: RaycastParams?): RaycastResult?",
        "function Spherecast(self, position: Vector3, radius: number, direction: Vector3, params: RaycastParams?): RaycastResult?",
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
}

# Hardcoded types
# These will go before anything else, and are useful to define for other tools
START_BASE = """
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
    getmemorycategory: () -> string,
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
"""

POST_DATATYPES_BASE = """
declare class SharedTable
  [string | number]: any
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
"""

# More hardcoded types, but go at the end of the file
# Useful if they rely on previously defined types
END_BASE = """
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
"""

SERVICES: List[str] = []  # All available services name
CREATABLE: List[str] = []  # All creatable instances
BRICK_COLORS: Set[str] = set()

@dataclass
class Security:
    read: str
    write: str

@dataclass
class Parameter:
    name: str
    type: str
    default: Optional[str]


@dataclass
class Constructor:
    name: str
    tags: list[str]
    parameters: list[Parameter]


@dataclass
class Property:
    name: str
    type: str
    tags: list[str]
    security: Security


@dataclass
class Function:
    name: str
    parameters: list[Parameter]
    returns: list[str]
    tags: list[str]
    security: Security


@dataclass
class MathOperation:
    operation: str
    type_a: str
    type_b: str
    return_type: str
    tags: list[str]


@dataclass
class DataType:
    name: str
    tags: list[str]
    constructors: list[Constructor]
    properties: list[Property]
    constants: list[Property]
    methods: list[Function]
    functions: list[Function]
    math_operations: list[MathOperation]


@dataclass
class Event:
    name: str
    parameters: list[Parameter]
    tags: list[str]
    security: Security


@dataclass
class EngineClass:
    name: str
    inherits: Optional[str]
    tags: list[str]
    properties: list[Property]
    methods: list[Function]
    events: list[Event]
    callbacks: list[Function]

@dataclass
class EnumItem:
    name: str
    tags: list[str]

@dataclass
class Enum:
    name: str
    tags: list[str]
    items: list[EnumItem]


ENUMS: list[Enum] = []

chosenSecurityLevel = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SECURITY_LEVEL
assert (
    chosenSecurityLevel in SECURITY_LEVELS
), f"Unknown security level: {chosenSecurityLevel}"


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


def resolveType(type: str) -> str:
    type = type.strip()

    if "|" in type:
        return " | ".join([resolveType(ty) for ty in type.split("|")])

    if any(enum.name == type for enum in ENUMS):
        return "Enum" + type

    if type.startswith("Array<") and type.endswith(">"):
        return "{ " + resolveType(type.removeprefix("Array<").removesuffix(">")) + " }"

    if type.startswith("Tuple<") and type.endswith(">"):
        return "..." + resolveType(type.removeprefix("Tuple<").removesuffix(">"))

    return TYPE_INDEX.get(type, type)
    # if "Generic" in type and type["Generic"] is not None:
    #     name = type["Generic"]
    #     if name.startswith("Enum."):
    #         name = "Enum" + name[5:]
    #     return "{ " + name + " }"

    # if "Tuple" in type and type["Tuple"] is not None:
    #     subtype = resolveType(type["Tuple"])
    #     return f"...({subtype})"
    # if "Variadic" in type and type["Variadic"] is not None:
    #     subtype = resolveType(type["Variadic"])
    #     return f"...{subtype}"

    # name, category = (
    #     type["Name"],
    #     type["Category"] if "Category" in type else "Primitive",
    # )


def resolveParameter(param: Parameter):
    if param.name == "...":
        return f"...: {resolveType(param.type.removeprefix("Tuple<").removesuffix(">"))}"

    return f"{escapeName(param.name)}: {resolveType(param.type)}{'?' if param.default else ''}"


def resolveReturnType(returns: list[str]) -> str:
    if len(returns) > 1:
        return_details = ", ".join([resolveType(ret) for ret in returns])
        return f"({return_details})"
    elif len(returns) == 0:
        return "()"
    else:
        return resolveType(returns[0])


def filterMember(klassName: str, member: Function):
    if not INCLUDE_DEPRECATED_METHODS and "Deprecated" in member.tags:
        return False
    if klassName in IGNORED_MEMBERS and member.name in IGNORED_MEMBERS[klassName]:
        return False
    if member.security:
        if min(SECURITY_LEVELS.index(member.security.read), SECURITY_LEVELS.index(member.security.write)) > SECURITY_LEVELS.index(chosenSecurityLevel):
            return False

    return True


def shouldExcludeAsDeprecated(klass: EngineClass):
    return (
        not INCLUDE_DEPRECATED_METHODS
        and "Deprecated" in klass.tags
        and not klass.name in OVERRIDE_DEPRECATED_REMOVAL
    )

def declareProperty(property: Property):
    return (
            f"\t{escapeName(property.name)}: {resolveType(property.type)}\n"
        )

def declareMethod(function: Function):
    parameter_details = ["self"] + [resolveParameter(param) for param in function.parameters]
    return f"\tfunction {escapeName(function.name)}({", ".join(parameter_details)}): {resolveReturnType(function.returns)}\n"

def declareEvent(event: Event):
    parameters = ", ".join([resolveType(parameter.type) for parameter in event.parameters])
    return f"\t{escapeName(event.name)}: RBXScriptSignal<{parameters}>\n"

def declareCallback(callback: Function):
    parameter_details = [resolveParameter(param) for param in callback.parameters]
    return f"\t{escapeName(callback.name)}: ({", ".join(parameter_details)}) -> {resolveReturnType(callback.returns)}\n"

def declareClass(engineClass: EngineClass) -> str:
    if engineClass.name in IGNORED_INSTANCES:
        return ""

    if shouldExcludeAsDeprecated(engineClass):
        return ""

    out = "declare class " + engineClass.name
    if engineClass.inherits:
        out += " extends " + engineClass.inherits
    out += "\n"

    memberDefinitions = [
        declareProperty(p) for p in engineClass.properties if filterMember(engineClass.name, p)
    ] + [
        declareMethod(m) for m in engineClass.methods  if filterMember(engineClass.name, m)
    ] + [
        declareEvent(e) for e in engineClass.events if filterMember(engineClass.name, e)
    ] + [
        declareCallback(c) for c in engineClass.callbacks if filterMember(engineClass.name, c)
    ]

    if engineClass.name in EXTRA_MEMBERS:
        memberDefinitions += [
            f"\t{member}\n" for member in EXTRA_MEMBERS[engineClass.name]
        ]

    out += "".join(sorted(memberDefinitions))
    out += "end"

    return out


def printEnums(enums: list[Enum]):
    # Declare each enum individually
    out = ""
    for enum in enums:
        # Declare an atom for the enum
        out += f"declare class Enum{enum.name} extends EnumItem end\n"
        out += f"declare class Enum{enum.name}_INTERNAL extends Enum\n"
        items = sorted(enum.items, key=lambda x: x.name)
        for item in items:
            out += f"\t{escapeName(item.name)}: Enum{enum.name}\n"
        out += "end\n"
    print(out)
    print()

    # Declare enums as a whole
    out = "type ENUM_LIST = {\n"
    for enum in enums:
        out += f"\t{enum.name}: Enum{enum.name}_INTERNAL,\n"
    out += "} & { GetEnums: (self: ENUM_LIST) -> { Enum } }\n"
    out += "declare Enum: ENUM_LIST"
    print(out)
    print()


def printClasses(klasses: list[EngineClass]):
    # Forward declare "deprecated" classes in case they are still used
    for klass in klasses:
        if shouldExcludeAsDeprecated(klass):
            print(f"export type {klass.name} = any")

    for klass in klasses:
        if klass.name in IGNORED_INSTANCES:
            continue

        print(declareClass(klass))
        print()

def metamethod(operator: str):
    if operator == "+":
        return "__add"
    elif operator == "-":
        return "__sub"
    elif operator == "*":
        return "__mul"
    elif operator == "/":
        return "__div"
    elif operator == "//":
        return "__idiv"
    assert False, operator


def printDataTypes(types: List[DataType]):
    for dataType in types:
        math_operations = [
            Function(metamethod(operation.operation) , [Parameter("other", operation.type_b, None)], [operation.return_type], [], Security("None", "None"))
            for operation in dataType.math_operations
        ]

        print(declareClass(EngineClass(dataType.name, None, dataType.tags, dataType.properties, dataType.methods + math_operations, [], [])))
        print()


def printDataTypeConstructors(types: List[DataType]):
    for klass in types:
        if klass.name in IGNORED_INSTANCES:
            continue

        isBrickColorNew = False

        # Handle overloadable functions
        functions: defaultdict[str, List[Function]] = defaultdict(list)
        for member in klass.constructors:
            if klass.name == "BrickColor" and member.name == "new" and len(member.parameters) == 1 and member.parameters[0].type == "string":
                isBrickColorNew = True
                continue

            functions[member.name].append(Function(member.name, member.parameters, [klass.name], member.tags, Security("None", "None")))

        for member in klass.functions:
            functions[member.name].append(member)

        if len(klass.constants) + len(functions) == 0:
            continue

        out = "declare " + klass.name + ": {\n"

        memberDefinitions = [
            f"\t{escapeName(constant.name)}: {resolveType(constant.type)},\n"
            for constant in klass.constants
        ]

        # Special case string BrickColor new
        if isBrickColorNew:
            colors = " | ".join(map(lambda c: f'"{c}"', sorted(BRICK_COLORS)))

            functions["new"].append(Function("new", [Parameter("name", colors, None)], ["BrickColor"], [], Security("None", "None")))

        for function, overloads in functions.items():
            overloads = [f"(({", ".join([resolveParameter(parameter) for parameter in member.parameters])}) -> {resolveReturnType(member.returns)})" for member in overloads]
            memberDefinitions.append(f"\t{escapeName(function)}: {' & '.join(overloads)},\n")

        out += "".join(sorted(memberDefinitions))
        out += "}"
        print(out)
        print()


def loadClassesIntoStructures(engineClasses: list[EngineClass]):
    for klass in engineClasses:
        if klass.name in IGNORED_INSTANCES:
            continue

        if "Deprecated" in klass.tags and not INCLUDE_DEPRECATED_METHODS and not klass.name in OVERRIDE_DEPRECATED_REMOVAL:
            continue

        if "Service" in klass.tags:
            SERVICES.append(klass.name)

        if "NotCreatable" not in klass.tags:
            CREATABLE.append(klass.name)


def processBrickColors(colors):
    for color in colors["BrickColors"]:
        BRICK_COLORS.add(color["Name"])


def printJsonPrologue():
    data = {"CREATABLE_INSTANCES": CREATABLE, "SERVICES": SERVICES}
    print("--#METADATA#" + json.dumps(data, indent=None))
    print()


# Load BrickColors
brickColors = json.loads(requests.get(BRICK_COLORS_URL).text)
processBrickColors(brickColors)

CLASSES_DIRECTORY = os.path.join(DOCUMENTATION_DIRECTORY, "classes")
DATATYPES_DIRECTORY = os.path.join(DOCUMENTATION_DIRECTORY, "datatypes")
ENUMS_DIRECTORY = os.path.join(DOCUMENTATION_DIRECTORY, "enums")

def parse_security(info) -> Security:
    security = info.get("security", None)
    if not security:
        return Security("None", "None")
    if isinstance(security, str):
        return Security(security, security)
    else:
        return Security(security["read"], security["write"])

def parse_parameter(info) -> Parameter:
    return Parameter(info["name"], info["type"], info["default"])


def parse_constructor(info) -> Constructor:
    _class_name, constructor_name = info["name"].split(".")
    parameters = (
        [parse_parameter(param) for param in info["parameters"]]
        if info["parameters"]
        else []
    )
    return Constructor(constructor_name, info["tags"], parameters)


def parse_property(info, is_global = False) -> Property:
    if is_global:
        property_name = info["name"]
    else:
        _class_name, property_name = info["name"].split(".")
    return Property(property_name, info["type"], info["tags"], parse_security(info))


def parse_method(info) -> Function:
    _class_name, method_name = info["name"].split(":")
    parameters = (
        [parse_parameter(param) for param in info["parameters"]]
        if info["parameters"]
        else []
    )
    returns = [ret["type"] for ret in info["returns"]]
    return Function(method_name, parameters, returns, info["tags"], parse_security(info))


def parse_function(info, is_global = False) -> Function:
    if is_global:
        function_name = info["name"]
    else:
        _class_name, function_name = info["name"].split(".")
    parameters = [parse_parameter(param) for param in info["parameters"]] if info["parameters"] else []
    returns = [ret["type"] for ret in info["returns"]]
    return Function(function_name, parameters, returns, info["tags"], parse_security(info))

def parse_event(info) -> Event:
    _class_name, event_name = info["name"].split(".")
    parameters = [parse_parameter(param) for param in info["parameters"]] if info["parameters"] else []
    return Event(event_name, parameters, info["tags"], parse_security(info))


def parse_math_operation(info) -> MathOperation:
    return MathOperation(
        info["operation"],
        info["type_a"],
        info["type_b"],
        info["return_type"],
        info["tags"],
    )

def parse_enum_item(info) -> EnumItem:
    return EnumItem(
        info["name"],
        info["tags"]
    )


DATATYPES: list[DataType] = []
CLASSES: list[EngineClass] = []
GLOBAL_VALUES: list[Property] = []
GLOBAL_FUNCTIONS: list[Function] = []

for file in os.listdir(DATATYPES_DIRECTORY):
    with open(os.path.join(DATATYPES_DIRECTORY, file)) as file:
        decoded = yaml.safe_load(file)
        assert decoded["type"] == "datatype"

        name = decoded["name"]
        tags = decoded["tags"]
        constructors = decoded["constructors"] or []
        properties = decoded["properties"] or []
        methods = decoded["methods"] or []
        functions = decoded["functions"] or []
        math_operations = decoded["math_operations"] or []
        constants = decoded["constants"] or []

        DATATYPES.append(
            DataType(
                name,
                tags,
                [parse_constructor(constructor) for constructor in constructors],
                [parse_property(property) for property in properties],
                [parse_property(constant) for constant in constants],
                [parse_method(method) for method in methods],
                [parse_function(function) for function in functions],
                [
                    parse_math_operation(math_operation)
                    for math_operation in math_operations
                ],
            )
        )

for file in os.listdir(CLASSES_DIRECTORY):
    with open(os.path.join(CLASSES_DIRECTORY, file)) as file:
        decoded = yaml.safe_load(file)
        assert decoded["type"] == "class"

        name = decoded["name"]
        inherits = decoded["inherits"]
        tags = decoded["tags"]
        properties = decoded["properties"] or []
        methods = decoded["methods"] or []
        events = decoded["events"] or []
        callbacks = decoded["callbacks"] or []

        if inherits:
            assert len(inherits) == 1
            inherits = inherits[0]

        CLASSES.append(
            EngineClass(
                name,
                inherits,
                tags,
                [parse_property(property) for property in properties],
                [parse_method(method) for method in methods],
                [parse_event(event) for event in events],
                [parse_function(function) for function in callbacks],
            )
        )


for file in os.listdir(ENUMS_DIRECTORY):
    with open(os.path.join(ENUMS_DIRECTORY, file)) as file:
        decoded = yaml.safe_load(file)
        assert decoded["type"] == "enum"

        name = decoded["name"]
        tags = decoded["tags"]
        items = decoded["items"] or []

        ENUMS.append(
            Enum(
                name,
                tags,
                [parse_enum_item(enum_item) for enum_item in items]
            )
        )

with open(os.path.join(DOCUMENTATION_DIRECTORY, "globals", "RobloxGlobals.yaml")) as file:
    decoded = yaml.safe_load(file)

    GLOBAL_VALUES = [parse_property(property, is_global=True) for property in decoded.get("properties", [])]
    GLOBAL_FUNCTIONS = [parse_function(function, is_global=True) for function in decoded.get("functions", [])]

# Load services and creatable instances
loadClassesIntoStructures(CLASSES)

printJsonPrologue()
print(START_BASE)
printEnums(sorted(ENUMS, key=lambda x: x.name))
printDataTypes(sorted(DATATYPES, key=lambda x: x.name))
print(POST_DATATYPES_BASE)
printClasses(sorted(CLASSES, key=lambda x: x.name))
printDataTypeConstructors(sorted(DATATYPES, key=lambda x: x.name))

for value in sorted(GLOBAL_VALUES, key=lambda x: x.name):
    print(f"declare {escapeName(value.name)}: {resolveType(value.type)}")
for value in sorted(GLOBAL_FUNCTIONS, key=lambda x: x.name):
    print(f"declare function {escapeName(value.name)}({", ".join([resolveParameter(param) for param in value.parameters])}): {resolveReturnType(value.returns)}")

print(END_BASE)
