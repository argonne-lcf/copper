--%Module
-- copy this modulefiles/copper/ directory to /sw/frontier/modulefiles/copper/0.1.lua
-- update the copper_root path below as needed

-- Basic info (optional but nice to have)
whatis("Name: copper")
whatis("Description: Copper cooperative caching tool (cu_fuse build)")
whatis("Category: I/O, caching")

local help_message = [[

Copper cooperative caching tools

Commands:
  launch_copper   -> launch_copper.sh (start Copper/cu_fuse)
  stop_copper     -> stop_copper.sh (stop Copper/cu_fuse)

Environment:
  COPPER_ROOT
  CUPATH
  CU_FUSE_MNT_VIEWDIR
  facility_address_book
]]

help(help_message, "\n")

-- Root
local copper_root = "/lustre/orion/gen008/proj-shared/kaushik/data_loading_tools/copper/copper"
setenv("COPPER_ROOT", copper_root)

-- PATH to build dir with binaries + scripts
prepend_path("PATH", copper_root .. "/build")

-- Other env
setenv("CUPATH", copper_root .. "/build/cu_fuse")
setenv("CU_FUSE_MNT_VIEWDIR", "/tmp/" .. (os.getenv("USER") or "user") .. "/copper")
setenv("facility_address_book", copper_root .. "/olcf_copper_addressbook.txt")

-- Nice aliases
set_alias("launch_copper", "launch_copper.sh")
set_alias("stop_copper",   "stop_copper.sh")

if (mode() == "load") then
    LmodMessage("Copper loaded: use 'launch_copper' and 'stop_copper'.")
end