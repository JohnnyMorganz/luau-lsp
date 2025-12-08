--#METADATA#{"CREATABLE_INSTANCES":["TextLabel","Part"], "SERVICES": ["ReplicatedStorage","ServerScriptService","Workspace"]}

declare class Enum
    function GetEnumItems(self): { any }
end
declare class EnumItem
    Name: string
    Value: number
    EnumType: Enum
    function IsA(self, enumName: string): boolean
end

declare class EnumHumanoidRigType extends EnumItem end
declare class EnumHumanoidRigType_INTERNAL extends Enum
    R6: EnumHumanoidRigType
    R15: EnumHumanoidRigType
end
type ENUM_LIST = {
    HumanoidRigType: EnumHumanoidRigType_INTERNAL,
} & { GetEnums: (self: ENUM_LIST) -> { Enum } }
declare Enum: ENUM_LIST

declare class RBXScriptConnection
    Connected: boolean
    function Disconnect(self): nil
end

export type RBXScriptSignal<T... = ...any> = {
    Wait: (self: RBXScriptSignal<T...>) -> T...,
    Connect: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    ConnectParallel: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
}

declare class Object
	Changed: RBXScriptSignal<string>
	ClassName: string
	function GetPropertyChangedSignal(self, property: string): RBXScriptSignal
	function IsA(self, className: string): boolean
end

declare class Instance extends Object
    AncestryChanged: RBXScriptSignal<Instance, Instance?>
    AttributeChanged: RBXScriptSignal<string>
    ChildAdded: RBXScriptSignal<Instance>
    ChildRemoved: RBXScriptSignal<Instance>
    DescendantAdded: RBXScriptSignal<Instance>
    DescendantRemoving: RBXScriptSignal<Instance>
    Destroying: RBXScriptSignal<>
    Name: string
    Parent: Instance?
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
    function GetAttribute(self, attribute: string): unknown?
    function GetAttributeChangedSignal(self, attribute: string): RBXScriptSignal<>
    function GetAttributes(self): { [string]: unknown }
    function GetChildren(self): { Instance }
    function GetDescendants(self): { Instance }
    function GetFullName(self): string
    function IsAncestorOf(self, descendant: Instance): boolean
    function IsDescendantOf(self, ancestor: Instance): boolean
    function SetAttribute(self, attribute: string, value: any): nil
    function WaitForChild(self, name: string): Instance
    function WaitForChild(self, name: string, timeout: number): Instance?
end

declare class Part extends Instance
    Anchored: boolean
end

declare class TextLabel extends Instance
    Text: string
end

declare class ReplicatedStorage extends Instance
end

declare class ServerScriptService extends Instance
end

declare class Workspace extends Instance
end

declare class ServiceProvider extends Instance
    function GetService(self, className: string): Instance
end

declare class DataModel extends ServiceProvider
    function BindToClose(self, func: ((...any) -> ...any)): nil
end

declare Instance: {
    new: ((className: string, parent: Instance?) -> Instance),
    fromExisting: ((existingInstance: Instance) -> Instance),
}

declare game: DataModel

--- This is a documented table
export type DocumentedTable = {
    --- This is documented member1 of the table
    member1: string,
}

--- This is a documented class
declare class DocumentedClass
    --- This is a documented member1 of the class
    member1: string
    --- This is a documented function1 of the class
    function function1(self): number
end

--- This is a documented global function
declare function DocumentedGlobalFunction(): number

--- This is a documented global variable
declare DocumentedGlobalVariable: number
