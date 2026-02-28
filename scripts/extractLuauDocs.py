from json import load, dump

with open("api-docs/en-us.json") as f:
    data: dict[str, dict[str, str]] = load(f)
    f.close()

luau_docs = {k: v for k, v in data.items() if k.startswith("@luau")}

with open("api-docs/luau-en-us.json", "w") as f:
    
    # changing link from the roblox docs site to luau docs site.
    for id, docs_entry in luau_docs.items():
        link = docs_entry.get("learn_more_link")
        
        if link is None:
            continue
        
        # if the roblox docs url is to a built-in library
        if link.startswith("https://create.roblox.com/docs/reference/engine/libraries/"):
            
            # find the index of the path separator ("/") and the fragment ("#")
            path_sep_idx = link.rfind("/")
            fragment_idx = link.rfind("#")
            
            # fragment index is corrected whenever finding the symbol fails
            fragment_idx = None if fragment_idx == -1 else fragment_idx + 1
            
            # add missing fragment symbol ("#") because the roblox docs link doesn't have it
            link = link[:path_sep_idx + 1] + "#" + link[path_sep_idx + 1:]
            
            # add to the computed dictionary, append "-library" because the luau site requires it
            luau_docs[id]["learn_more_link"] = "https://luau.org/library/" + link[path_sep_idx + 1:fragment_idx] + "-library"
            
        # the only other case is that the url is to a global function
        else:
            luau_docs[id]["learn_more_link"] = "https://luau.org/library/#global-functions"
    dump(luau_docs, f, indent=4)
    f.close()