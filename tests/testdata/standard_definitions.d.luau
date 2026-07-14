--#METADATA#{"CREATABLE_INSTANCES":["TextLabel","Part"], "SERVICES": ["ReplicatedStorage","ServerScriptService","Workspace"]}

declare extern type Enum with
    function GetEnumItems(self): { any }
end
declare extern type EnumItem with
    Name: string
    Value: number
    EnumType: Enum
    function IsA(self, enumName: string): boolean
end

declare extern type EnumHumanoidRigType extends EnumItem with end
declare extern type EnumHumanoidRigType_INTERNAL extends Enum with
    R6: EnumHumanoidRigType
    R15: EnumHumanoidRigType
end
type ENUM_LIST = {
    HumanoidRigType: EnumHumanoidRigType_INTERNAL,
} & { GetEnums: (self: ENUM_LIST) -> { Enum } }
declare Enum: ENUM_LIST

declare extern type RBXScriptConnection with
    Connected: boolean
    function Disconnect(self): nil
end

export type RBXScriptSignal<T... = ...any> = {
    Wait: (self: RBXScriptSignal<T...>) -> T...,
    Connect: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
    ConnectParallel: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
}

declare extern type Object with
	Changed: RBXScriptSignal<string>
	ClassName: string
	function GetPropertyChangedSignal(self, property: string): RBXScriptSignal
	function IsA(self, className: string): boolean
end

declare extern type Instance extends Object with
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
    function IsPropertyModified(self, property: string): boolean
    function ResetPropertyToDefault(self, property: string): nil
    function SetAttribute(self, attribute: string, value: any): nil
    function WaitForChild(self, name: string): Instance
    function WaitForChild(self, name: string, timeout: number): Instance?
    function QueryDescendants(self, selector: string): { Instance }
end

declare extern type BasePart extends Instance with
    Anchored: boolean
end

declare extern type Part extends BasePart with
    Anchored: boolean
end

declare extern type TextLabel extends Instance with
    Text: string
end

declare extern type ReplicatedStorage extends Instance with
end

declare extern type ServerScriptService extends Instance with
end

declare extern type Workspace extends Instance with
end

declare extern type ServiceProvider extends Instance with
    function GetService(self, className: string): Instance
end

declare extern type DataModel extends ServiceProvider with
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
declare extern type DocumentedClass with
    --- This is a documented member1 of the class
    member1: string
    --- This is a documented function1 of the class
    function function1(self): number
end

--- This is a documented global function
declare function DocumentedGlobalFunction(): number

--- This is a documented global variable
declare DocumentedGlobalVariable: number
