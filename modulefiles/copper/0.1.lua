--%Module
-- update the copper_root path below as needed
-- Basic info (optional but nice to have)

whatis("Name: copper")
whatis("Description: Copper cooperative caching tool (cu_fuse build)")
whatis("Category: I/O, caching")

local help_message = [[

Copper cooperative caching tools

Commands:
  launch_copper            -> launch_copper_frontier.sh (start Copper/cu_fuse on Frontier)
  stop_copper              -> stop_copper_frontier.sh (stop Copper/cu_fuse on Frontier)
  launch_copper_frontier   -> launch_copper_frontier.sh
  stop_copper_frontier     -> stop_copper_frontier.sh

Environment:
  COPPER_ROOT
  CUPATH
  CU_FUSE_MNT_VIEWDIR
  facility_address_book
  build/olcf_frontier_copper_addressbook.txt
  build/alcf_aurora_copper_addressbook.txt
]]

help(help_message, "\n")

-- Root
local copper_root = "/sw/frontier/ums/ums046/copper"
setenv("COPPER_ROOT", copper_root)

-- PATH to build dir with binaries + scripts
prepend_path("PATH", copper_root .. "/build")

-- Other env
setenv("CUPATH", copper_root .. "/build/cu_fuse")
setenv("CU_FUSE_MNT_VIEWDIR", "/tmp/" .. (os.getenv("USER") or "user") .. "/copper")
setenv("facility_address_book", copper_root .. "/build/olcf_frontier_copper_addressbook.txt")

-- Nice aliases
set_alias("launch_copper",          "launch_copper_frontier.sh")
set_alias("stop_copper",            "stop_copper_frontier.sh")
set_alias("launch_copper_frontier", "launch_copper_frontier.sh")
set_alias("stop_copper_frontier",   "stop_copper_frontier.sh")

if (mode() == "load") then
    LmodMessage("Copper loaded: use 'launch_copper' and 'stop_copper'.")
    LmodMessage("COPPER_ROOT: " .. copper_root)
    LmodMessage("launch_copper points to: " .. copper_root .. "/build/launch_copper_frontier.sh")
end
