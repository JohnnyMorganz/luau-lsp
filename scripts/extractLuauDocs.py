from json import load, dump

PREFIX_LENGTH = 7

with open("api-docs/en-us.json") as f:
    data = load(f)

luau_docs = {k: v for k, v in data.items() if k.startswith("@luau")}
vector_lib_docs = {k: v for k, v in data.items() if k.startswith("@roblox/global/vector")}
new_vector_lib_docs = {}

# Changes docs entries for the vector library to be consistent with other entries.
for docs_key, docs_entry in vector_lib_docs.items():
    new_vector_lib_docs["@luau" + docs_key[PREFIX_LENGTH:]] = docs_entry
    
    param_val = docs_entry.get("params")
    ret_val = docs_entry.get("returns")
    
    if param_val is None:
        continue
    
    # Correct the params
    for param_entry in param_val:
        param_docs = param_entry["documentation"]
        param_entry["documentation"] = "@luau" + param_docs[PREFIX_LENGTH:]
    
    # Correct the return value.
    val = ret_val[0]
    ret_val[0] = "@luau" + ret_val[0][PREFIX_LENGTH:]

# Correct the keys.
for k, v in vector_lib_docs["@roblox/global/vector"]["keys"].items():
    new_vector_lib_docs["@luau/global/vector"]["keys"][k] = "@luau" + v[PREFIX_LENGTH:]

luau_docs.update(new_vector_lib_docs)

# Changes the URL from directing to the roblox docs site to the luau.org site.
for docs_key, docs_entry in luau_docs.items():
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
        luau_docs[docs_key]["learn_more_link"] = "https://luau.org/library/" + link[path_sep_idx + 1:fragment_idx] + "-library"
        
    # Otherwise, it's from a global function.
    else:
        luau_docs[docs_key]["learn_more_link"] = "https://luau.org/library/#global-functions"

with open("api-docs/luau-en-us.json", "w") as f:
    dump(luau_docs, f, indent=2)