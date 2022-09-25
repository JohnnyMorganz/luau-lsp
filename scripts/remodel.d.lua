declare class RemodelInstance
	Name: string
	ClassName: string
	Parent: RemodelInstance
	function Destroy(self): ()
	function Clone(self): RemodelInstance
	function GetChildren(self): { RemodelInstance }
	function GetDescendants(self): { RemodelInstance }
	function FindFirstChild(self, name: string): RemodelInstance?
end

declare class RemodelDataModel extends RemodelInstance
	function GetService(self, name: string): RemodelInstance
end

type SupportedType = "String" | "Content" | "Bool" | "Float64" | "Float32" | "Int64" | "Int32"

export type Remodel = {
	readPlaceFile: (path: string) -> RemodelDataModel,
	readModelFile: (path: string) -> { RemodelInstance },
	readPlaceAsset: (assetId: string) -> RemodelDataModel,
	readModelAsset: (assetId: string) -> { RemodelInstance },
	writePlaceFile: (path: string, instance: RemodelDataModel)	-> (),
	writeModelFile: (path: string, instance: RemodelInstance) -> (),
	writeExistingPlaceAsset: (instance: RemodelDataModel, assetId: string) -> (),
	writeExistingModelAsset: (instance: RemodelInstance, assetId: string) -> (),
	getRawProperty: (instance: RemodelInstance, name: string) -> any?,
	setRawProperty: (instance: RemodelInstance, name: string, type: SupportedType, value: any) -> (),
	readFile: (path: string) -> string,
	readDir: (path: string) -> { string },
	writeFile: (path: string, contents: string) -> (),
	removeFile: (path: string) -> (),
	createDirAll: (path: string) -> (),
	removeDir: (path: string) -> (),
	isFile: (path: string) -> boolean,
	isDir: (path: string) -> boolean,
}

export type JSON = {
	fromString: (source: string) -> any,
	toString: (value: any) -> string,
	toStringPretty: (value: any, indent: string?) -> string,
}

declare Instance: {
	new: (className: string) -> RemodelInstance,
}

declare remodel: Remodel
declare json: JSON