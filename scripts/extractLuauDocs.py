from json import load, dump

with open("api-docs/en-us.json") as f:
    data = load(f)

luau_docs = {k: v for k, v in data.items() if k.startswith("@luau")}
vector_lib_docs = {k: v for k, v in data.items() if k.startswith("@roblox/global/vector")}
new_vector_lib_docs = {}

# changes docs keys and entries for the vector library to be consistent with other entries.
for docs_key, docs_entry in vector_lib_docs.items():
    new_vector_lib_docs[docs_key.replace("@roblox", "@luau")] = docs_entry
    
    param_val = docs_entry.get("params")
    ret_val = docs_entry.get("returns")
    
    # no param = no return -> no need to correct anything else.
    if param_val is None:
        continue
    
    # correct the params to @luau.
    for param_entry in param_val:
        param_entry["documentation"] = param_entry["documentation"].replace("@roblox", "@luau")
    
    # correct the return to @luau.
    ret_val[0] = ret_val[0].replace("@roblox", "@luau")

# correct the docs keys to @luau.
for key, value in vector_lib_docs["@roblox/global/vector"]["keys"].items():
    new_vector_lib_docs["@luau/global/vector"]["keys"][key] = value.replace("@roblox", "@luau")

luau_docs.update(new_vector_lib_docs)

# redirect the URL from the create.roblox.com site to the luau.org site.
for docs_key, docs_entry in luau_docs.items():
    link = docs_entry.get("learn_more_link")
    
    if link is None:
        continue
    
    # if the URL is from a built-in library.
    if link.startswith("https://create.roblox.com/docs/reference/engine/libraries/"):
        
        # find the index from the right of the path separator ("/") and the fragment ("#")
        path_sep_idx = link.rfind("/")
        fragment_idx = link.rfind("#")
        
        # corrects the fragment index if the fragment is found.
        # this is only the case for directing to specific functions.
        fragment_idx = None if fragment_idx == -1 else fragment_idx + 1
        
        # add the missing fragment symbol ("#") because the roblox docs link doesn't have it,
        # while the luau.org link does.
        link = f"{link[:path_sep_idx + 1]}#{link[path_sep_idx + 1:]}"
        
        # formats the new link.
        luau_docs[docs_key]["learn_more_link"] = f"https://luau.org/library/{link[path_sep_idx + 1:fragment_idx]}-library"
        
    # otherwise, it's from a global function. cannot do more than this.
    else:
        luau_docs[docs_key]["learn_more_link"] = "https://luau.org/library/#global-functions"

# replacing `require()` description with a general one. this new description is based off the original description.
luau_docs["@luau/global/require/param/0"]["documentation"] = "The directory path to a file or a folder/directory to retrieve the return value it provides. Directory path can use aliases defined in <code>.luaurc</code> or <code>.config.luau</code>. If the path is a file, executes that file. If the path is a folder/directory, finds <code>init.luau</code> or <code>init.lua</code> and execute that instead."
luau_docs["@luau/global/require/return/0"]["documentation"] = "What the file or folder/directory returned (usually a table or a function)."
luau_docs["@luau/global/require"]["documentation"] = "Returns the value that was returned by the given directory path, running it if it has not been run yet."

with open("api-docs/luau-en-us.json", "w") as f:
    dump(luau_docs, f, indent=2)