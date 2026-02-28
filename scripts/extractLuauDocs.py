import json

with open("api-docs/en-us.json") as f:
    data = json.load(f)

luau_docs = {k: v for k, v in data.items() if k.startswith("@luau")}

with open("api-docs/luau-en-us.json", "w") as f:
    json.dump(luau_docs, f)
