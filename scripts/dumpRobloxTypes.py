# Script to pull in API Dump and export it into a definition file
# Based off https://gist.github.com/HawDevelopment/97f2411149e24d8e7a712016114d55ff
from typing import List, Literal, Optional, Union, TypedDict
from collections import defaultdict
import requests
import json

# API Endpoints
DATA_TYPES_URL = "https://raw.githubusercontent.com/NightrainsRbx/RobloxLsp/master/server/api/DataTypes.json"
API_DUMP_URL = "https://raw.githubusercontent.com/CloneTrooper1019/Roblox-Client-Tracker/roblox/API-Dump.json"
CORRECTIONS_URL = "https://raw.githubusercontent.com/NightrainsRbx/RobloxLsp/master/server/api/Corrections.json"

INCLUDE_DEPRECATED_METHODS = False
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
    "Function": "(...any) -> ...any",
    "function": "(...any) -> ...any",
    "bool": "boolean",
    "int": "number",
    "int64": "number",
    "double": "number",
    "float": "number",
    "void": "nil",
    "Objects": "{ Instance }",
    "Dictionary": "{ [any]: any }",
    "Map": "{ [any]: any }",
    "Array": "{ any }",
    "table": "{ any }",
    "CoordinateFrame": "CFrame",
}

IGNORED_INSTANCES: List[str] = [
    "RBXScriptSignal",  # Redefined using generics
    "BlockMesh",  # its superclass is marked as deprecated but it isn't, so its broken
    "Enum",  # redefined explicitly
    "EnumItem",  # redefined explicitly
    "GlobalSettings",  # redefined explicitly
]

# These classes are deferred to the very end of the dump, so that they have access to all the types
DEFERRED_CLASSES: List[str] = [
    "ServiceProvider",
    # The following must be deferred as they rely on ServiceProvider
    "DataModel",
    "GenericSettings",
    "AnalysticsSettings",
    "UserSettings",
    # Plugin is deferred after its items are declared
    "Plugin",
]

# Methods / Properties ignored in classes. Commonly used to add corrections
IGNORED_MEMBERS = {
    "Instance": [
        "Parent",
        "FindFirstChild",
        "FindFirstAncestor",
        "FindFirstDescendant",
        "GetActor",
        "WaitForChild",
    ],
    "Model": ["PrimaryPart"],
    "RemoteEvent": [
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
    "Players": ["GetPlayers"],
    "ContextActionService": ["BindAction", "BindActionAtPriority"],
    "WorldRoot": ["Raycast"],
    "HttpService": ["RequestAsync"],
    "HumanoidDescription": [
        "GetAccessories",
        "SetAccessories",
        "GetEmotes",
        "SetEmotes",
        "GetEquippedEmotes",
        "SetEquippedEmotes",
    ],
    "TeleportService": [
        "GetPlayerPlaceInstanceAsync",
        "TeleportAsync",
        "TeleportPartyAsync",
        "TeleportToPrivateServer",
        "ReserveServer",
        "LocalPlayerArrivedFromTeleport",
        "TeleportInitFailed",
    ],
}

# Extra members to add in to classes, commonly used to add in metamethods, and add corrections
EXTRA_MEMBERS = {
    "Vector3": [
        "function __add(self, other: Vector3): Vector3",
        "function __sub(self, other: Vector3): Vector3",
        "function __mul(self, other: Vector3 | number): Vector3",
        "function __div(self, other: Vector3 | number): Vector3",
        "function __unm(self): Vector3",
    ],
    "Vector2": [
        "function __add(self, other: Vector2): Vector2",
        "function __sub(self, other: Vector2): Vector2",
        "function __mul(self, other: Vector2 | number): Vector2",
        "function __div(self, other: Vector2 | number): Vector2",
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
    "UserSettings": [
        "GameSettings: UserGameSettings",
        'function GetService(self, service: "UserGameSettings"): UserGameSettings',
    ],
    "Instance": [
        "Parent: Instance?",
        "function FindFirstAncestor(self, name: string): Instance?",
        "function FindFirstChild(self, name: string, recursive: boolean?): Instance?",
        "function FindFirstDescendant(self, name: string): Instance?",
        "function GetActor(self): Actor?",
        "function WaitForChild(self, name: string): Instance",
        "function WaitForChild(self, name: string, timeout: number): Instance?",
    ],
    "Model": ["PrimaryPart: BasePart?"],
    "RemoteEvent": [
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
    "Players": ["function GetPlayers(self): { Player }"],
    "ContextActionService": [
        "function BindAction(self, actionName: string, functionToBind: (actionName: string, inputState: EnumUserInputState, inputObject: InputObject) -> EnumContextActionResult?, createTouchButton: boolean, ...: EnumUserInputType | EnumKeyCode): ()",
        "function BindActionAtPriority(self, actionName: string, functionToBind: (actionName: string, inputState: EnumUserInputState, inputObject: InputObject) -> EnumContextActionResult?, createTouchButton: boolean, priorityLevel: number, ...: EnumUserInputType | EnumKeyCode): ()",
    ],
    "Plugin": [
        "function CreateToolbar(self, name: string): PluginToolbar",
    ],
    "PluginToolbar": [
        "function CreateButton(self, id: string, toolTip: string, iconAsset: string, text: string?): PluginToolbarButton",
    ],
    "WorldRoot": [
        "function Raycast(self, origin: Vector3, direction: Vector3, raycastParams: RaycastParams?): RaycastResult?"
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
        "function GetPlayerPlaceInstanceAsync(self, userId: number): (boolean, string, number, string)",
        "function TeleportAsync(self, placeId: number, players: { Player }, teleportOptions: TeleportOptions?): TeleportAsyncResult",
        "function TeleportPartyAsync(self, placeId: number, players: { Player }, teleportData: any, customLoadingScreen: GuiObject?): string",
        "function TeleportToPrivateServer(self, placeId: number, reservedServerAccessCode: string, players: { Player }, spawnName: string?, teleportData: any, customLoadingScreen: GuiObject?): nil",
        "function ReserveServer(self, placeId: number): (string, string)",
        "LocalPlayerArrivedFromTeleport: RBXScriptSignal<Player, any>",
        "TeleportInitFailed: RBXScriptSignal<Player, EnumTeleportResult, string, number, TeleportOptions>",
    ],
}

# Hardcoded types
# These will go before anything else, and are useful to define for other tools
START_BASE = """
type Content = string
type ProtectedString = string
type BinaryString = string
type QDir = string
type QFont = string
type FloatCurveKey = any
type RotationCurveKey = any
type Instance = any

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
"""

# Hardcoded types after data types have been defined
POST_DATATYPES_BASE = """
export type RBXScriptSignal<T... = ...any> = {
    Wait: (self: RBXScriptSignal<T...>) -> T...,
    Connect: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    ConnectParallel: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
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

declare game: DataModel
declare workspace: Workspace
declare plugin: Plugin
declare script: LuaSourceContainer
declare function settings(): GlobalSettings
declare function UserSettings(): UserSettings
"""

CLASSES = {}  # All loaded classes from the API Dump, including corrections
SERVICES: List[str] = []  # All available services name
CREATABLE: List[str] = []  # All creatable instances

# Type Hints

CorrectionsValueType = TypedDict(
    "CorrectionsValueType",
    {
        "Name": str,
        "Category": None,
        "Default": Optional[str],
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
        "ReturnType": Optional[
            CorrectionsValueType
        ],  # TODO: it can also be { "Generic": "X" }, which I think signifies an array or smth?
        "TupleReturns": Optional[List[CorrectionsValueType]],
    },
)

CorrectionsClass = TypedDict(
    "CorrectionsClass", {"Name": str, "Members": List[CorrectionsMember]}
)

CorrectionsDump = TypedDict("CorrectionsDump", {"Classes": List[CorrectionsClass]})

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

ApiProperty = TypedDict(
    "ApiProperty",
    {
        "Name": str,
        "MemberType": Literal["Property"],
        "Description": Optional[str],
        "Tags": Optional[List[str]],  # TODO: stricter type?
        "Category": str,  # TODO: stricter type?
        "ValueType": ApiValueType,
    },
)

ApiFunction = TypedDict(
    "ApiFunction",
    {
        "Name": str,
        "MemberType": Literal["Function"],
        "Description": Optional[str],
        "Parameters": List[ApiParameter],
        "ReturnType": ApiValueType,
        "TupleReturns": Optional[CorrectionsValueType],
        "Tags": Optional[List[str]],  # TODO: stricter type?
    },
)

ApiEvent = TypedDict(
    "ApiEvent",
    {
        "Name": str,
        "MemberType": Literal["Event"],
        "Description": Optional[str],
        "Parameters": List[ApiParameter],
        "Tags": Optional[List[str]],  # TODO: stricter type?
    },
)

ApiCallback = TypedDict(
    "ApiCallback",
    {
        "Name": str,
        "MemberType": Literal["Callback"],
        "Description": Optional[str],
        "Parameters": List[ApiParameter],
        "ReturnType": ApiValueType,
        "TupleReturns": Optional[CorrectionsValueType],
        "Tags": Optional[List[str]],  # TODO: stricter type?
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
        "Tags": Optional[List[str]],  # TODO: stricter type?
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


def escapeName(name: str):
    """Escape a name string to be property-compatible"""
    if name == "function":
        return "func"
    return (
        name.replace(" ", "_")
        .replace("-", "")
        .replace('"', "")
        .replace("(", "")
        .replace(")", "")
        .replace("/", "_")
    )


def resolveType(type: Union[ApiValueType, CorrectionsValueType]) -> str:
    name, category = (
        type["Name"],
        type["Category"] if "Category" in type else "Primitive",
    )

    if name[-1] == "?":
        return resolveType({"Name": name[:-1], "Category": category}) + "?"

    if category == "Enum":
        return "Enum" + name
    else:
        return TYPE_INDEX[name] if name in TYPE_INDEX else name


def resolveParameter(param: ApiParameter):
    paramType = resolveType(param["Type"])
    isOptional = paramType[-1] == "?"
    return f"{escapeName(param['Name'])}: {paramType}{'?' if 'Default' in param and not isOptional else ''}"


def resolveParameterList(params: List[ApiParameter]):
    return ", ".join(map(resolveParameter, params))


def resolveReturnType(member: Union[ApiFunction, ApiCallback]):
    return (
        "(" + ", ".join(map(resolveType, member["TupleReturns"])) + ")"
        if "TupleReturns" in member
        else resolveType(member["ReturnType"])
    )


def declareClass(klass: ApiClass):
    if klass["Name"] in IGNORED_INSTANCES:
        return ""

    if (
        not INCLUDE_DEPRECATED_METHODS
        and "Tags" in klass
        and "Deprecated" in klass["Tags"]
        and not klass["Name"] in OVERRIDE_DEPRECATED_REMOVAL
    ):
        return ""

    out = "declare class " + klass["Name"]
    if "Superclass" in klass and klass["Superclass"] != "<<<ROOT>>>":
        out += " extends " + klass["Superclass"]
    out += "\n"

    isGetService = False

    for member in klass["Members"]:
        if (
            not INCLUDE_DEPRECATED_METHODS
            and "Tags" in member
            and "Deprecated" in member["Tags"]
        ):
            continue
        if (
            klass["Name"] in IGNORED_MEMBERS
            and member["Name"] in IGNORED_MEMBERS[klass["Name"]]
        ):
            continue

        if member["MemberType"] == "Property":
            out += (
                f"\t{escapeName(member['Name'])}: {resolveType(member['ValueType'])}\n"
            )
        elif member["MemberType"] == "Function":
            if klass["Name"] == "ServiceProvider" and member["Name"] == "GetService":
                isGetService = True
                continue
            out += f"\tfunction {escapeName(member['Name'])}(self{', ' if len(member['Parameters']) > 0 else ''}{resolveParameterList(member['Parameters'])}): {resolveReturnType(member)}\n"
        elif member["MemberType"] == "Event":
            parameters = ", ".join(
                map(lambda x: resolveType(x["Type"]), member["Parameters"])
            )
            out += f"\t{escapeName(member['Name'])}: RBXScriptSignal<{parameters}>\n"
        elif member["MemberType"] == "Callback":
            out += f"\t{escapeName(member['Name'])}: ({resolveParameterList(member['Parameters'])}) -> {resolveReturnType(member)}\n"

    # Special case ServiceProvider:GetService()
    if isGetService:
        for service in SERVICES:
            out += f'\tfunction GetService(self, service: "{service}"): {service}\n'

    if klass["Name"] in EXTRA_MEMBERS:
        for method in EXTRA_MEMBERS[klass["Name"]]:
            out += f"\t{method}\n"

    out += "end"

    return out


def printEnums(dump: ApiDump):
    enums: defaultdict[str, List[str]] = defaultdict(list)
    for enum in dump["Enums"]:
        for item in enum["Items"]:
            enums[enum["Name"]].append(item["Name"])

    # Declare each enum individually
    out = ""
    for enum, items in enums.items():
        # Declare an atom for the enum
        out += f"declare class Enum{enum} extends EnumItem end\n"
        out += f"declare class Enum{enum}_INTERNAL extends Enum\n"
        for item in items:
            out += f"\t{item}: Enum{enum}\n"
        out += "end\n"
    print(out)
    print()

    # Declare enums as a whole
    out = "declare Enum: {\n"
    for enum in enums:
        out += f"\t{enum}: Enum{enum}_INTERNAL,\n"
    out += "}"
    print(out)
    print()


def printClasses(dump: ApiDump):
    # Forward declare all the types
    for klass in dump["Classes"]:
        if klass["Name"] in IGNORED_INSTANCES:
            continue
        if klass["Name"] != "Instance":
            print(f"type {klass['Name']} = any")

    for klass in dump["Classes"]:
        if klass["Name"] in DEFERRED_CLASSES or klass["Name"] in IGNORED_INSTANCES:
            continue

        print(declareClass(klass))
        print()

    for klassName in DEFERRED_CLASSES:
        print(declareClass(CLASSES[klassName]))
        print()


def printDataTypes(types: List[DataType]):
    for klass in types:
        if klass["Name"] in IGNORED_INSTANCES:
            continue
        name = klass["Name"]
        members = klass["Members"]

        out = "declare class " + name + "\n"
        for member in members:
            if member["MemberType"] == "Property":
                out += f"\t{escapeName(member['Name'])}: {resolveType(member['ValueType'])}\n"
            elif member["MemberType"] == "Function":
                out += f"\tfunction {escapeName(member['Name'])}(self{', ' if len(member['Parameters']) > 0 else ''}{resolveParameterList(member['Parameters'])}): {resolveReturnType(member)}\n"
            elif member["MemberType"] == "Event":
                out += f"\t{escapeName(member['Name'])}: RBXScriptSignal\n"  # TODO: type this
            elif member["MemberType"] == "Callback":
                out += f"\t{escapeName(member['Name'])}: ({resolveParameterList(member['Parameters'])}) -> {resolveReturnType(member)}\n"

        if name in EXTRA_MEMBERS:
            for method in EXTRA_MEMBERS[name]:
                out += f"\t{method}\n"

        out += "end"
        print(out)
        print()


def printDataTypeConstructors(types: DataTypesDump):
    for klass in types["Constructors"]:
        if klass["Name"] in IGNORED_INSTANCES:
            continue
        name = klass["Name"]
        members = klass["Members"]

        isInstanceNew = False

        # Handle overloadable functions
        functions: defaultdict[str, List[ApiFunction]] = defaultdict(list)
        for member in members:
            if member["MemberType"] == "Function":
                if name == "Instance" and member["Name"] == "new":
                    isInstanceNew = True
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

        # Special case instance new
        if isInstanceNew:
            functions["new"] = list(
                map(
                    lambda inst: {
                        "Parameters": [
                            {
                                "Name": "className",
                                "Type": {
                                    "Name": f'"{inst}"',
                                    "Category": "PRIMITIVE_SERVICE_NAME",
                                },
                            }
                        ],
                        "ReturnType": {"Name": inst, "Category": "PRIMITIVE_SERVICE"},
                    },
                    CREATABLE,
                )
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
                                del otherMember["ReturnType"]
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

                            if "Parameters" in member:
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
                                                    otherParam["Type"][
                                                        "Generic"
                                                    ] = param["Type"]["Generic"]
                                            if "Default" in param:
                                                otherParam["Default"] = param["Default"]

                            break
                break


def loadClassesIntoStructures(dump: ApiDump):
    for klass in dump["Classes"]:
        if klass["Name"] in IGNORED_INSTANCES:
            continue

        isCreatable = True
        if "Tags" in klass:
            if (
                "Deprecated" in klass
                and not INCLUDE_DEPRECATED_METHODS
                and not klass["Name"] in OVERRIDE_DEPRECATED_REMOVAL
            ):
                continue
            if "Service" in klass["Tags"]:
                SERVICES.append(klass["Name"])
            if "NotCreatable" in klass["Tags"]:
                isCreatable = False
        if isCreatable:
            CREATABLE.append(klass["Name"])

        CLASSES[klass["Name"]] = klass


def topologicalSortDataTypes(dataTypes: List[DataType]) -> List[DataType]:
    # A child of node in graph indicates that the child references the node
    graph: defaultdict[str, set[str]] = defaultdict(set)

    dataTypeNames = {klass["Name"] for klass in dataTypes}

    def resolveClass(type: Union[ApiValueType, CorrectionsValueType]):
        name = type["Name"][:-1] if type["Name"][-1] == "?" else type["Name"]
        if name in dataTypeNames:
            return name

        return None

    def createReference(klassName: str, referenced: str | None):
        if referenced is not None:
            if klassName == referenced:
                return
            graph[referenced].add(klassName)

    for klass in dataTypes:
        klassName = klass["Name"]

        if klassName not in graph:
            graph[klassName] = set()

        for member in klass["Members"]:
            if member["MemberType"] == "Property":
                createReference(klassName, resolveClass(member["ValueType"]))
            elif (
                member["MemberType"] == "Function" or member["MemberType"] == "Callback"
            ):
                for param in member["Parameters"]:
                    createReference(klassName, resolveClass(param["Type"]))
                if "TupleReturns" in member:
                    for ret in member["TupleReturns"]:
                        createReference(klassName, resolveClass(ret))
                else:
                    createReference(klassName, resolveClass(member["ReturnType"]))
            elif member["MemberType"] == "Event":
                for param in member["Parameters"]:
                    createReference(klassName, resolveClass(param["Type"]))

    visited: set[str] = set()
    tempMark: set[str] = set()
    sort: List[DataType] = []

    def visit(klass: DataType):
        n = klass["Name"]

        if n in visited:
            return

        if n in tempMark:
            raise RuntimeError()

        tempMark.add(n)

        for child in sorted(graph[n]):
            visit(next(filter(lambda d: d["Name"] == child, dataTypes)))

        tempMark.remove(n)
        visited.add(n)
        sort.append(klass)

    stack = list(sorted(dataTypes, key=lambda x: x["Name"]))

    while stack:
        klass = stack.pop()
        if klass["Name"] not in visited:
            visit(klass)

    return list(reversed(sort))


# Print global types
dataTypes: DataTypesDump = json.loads(requests.get(DATA_TYPES_URL).text)
dump: ApiDump = json.loads(requests.get(API_DUMP_URL).text)

# Load services and creatable instances
loadClassesIntoStructures(dump)

# Apply any corrections on the dump
corrections: CorrectionsDump = json.loads(requests.get(CORRECTIONS_URL).text)
applyCorrections(dump, corrections)

print(START_BASE)
printEnums(dump)
printDataTypes(topologicalSortDataTypes(dataTypes["DataTypes"]))
print(POST_DATATYPES_BASE)
printClasses(dump)
printDataTypeConstructors(dataTypes)
print(END_BASE)
