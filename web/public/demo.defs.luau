--#METADATA#{"CREATABLE_INSTANCES": []}
export type DemoTypeFoo = string
export type DemoTypeBar = number

export type DemoTypeComposite = {
    foo: DemoTypeFoo | nil,
    bar: DemoTypeBar | nil
}

export type DemoTypeInterface = {
    fake_function : (value : DemoTypeComposite) -> (DemoTypeComposite),
}

declare demo: DemoTypeInterface
