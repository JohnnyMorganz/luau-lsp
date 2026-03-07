from json import load, dump

with open("api-docs/en-us.json") as f:
    data: dict[str, dict[str, str]] = load(f)

luau_docs = {k: v for k, v in data.items() if k.startswith("@luau")}

# Changes the URL from directing to the roblox docs site to the luau.org site.
for id, docs_entry in luau_docs.items():
    link = docs_entry.get("learn_more_link")
    
    if link is None:
        continue
    
    # If the URL is from a built-in library.
    if link.startswith("https://create.roblox.com/docs/reference/engine/libraries/"):
        
        # Find the index from the right of the path separator ("/") and the fragment ("#")
        path_sep_idx = link.rfind("/")
        fragment_idx = link.rfind("#")
        
        # Corrects the fragment index if the fragment is found.
        # This is only the case for directing to specific functions.
        fragment_idx = None if fragment_idx == -1 else fragment_idx + 1
        
        # Add the missing fragment symbol ("#") because the roblox docs link doesn't have it,
        # while the luau.org link does.
        link = link[:path_sep_idx + 1] + "#" + link[path_sep_idx + 1:]
        
        # Add formats the new link.
        luau_docs[id]["learn_more_link"] = "https://luau.org/library/" + link[path_sep_idx + 1:fragment_idx] + "-library"
        
    # Otherwise, it's from a global function.
    else:
        luau_docs[id]["learn_more_link"] = "https://luau.org/library/#global-functions"

with open("api-docs/luau-en-us.json", "w") as f:
    dump(luau_docs, f, indent=2)
    f.close()