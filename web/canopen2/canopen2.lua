--[[
 -------------------------------------------------------
 This page is a list of the CANopen nodes - for canopen2

 Author: Bjartey Sigurðardóttir (bjartey.sigurdardottir@marel.com) April 2014
]]--


local sm = require 'pluto.sharedmemory'

function readMemory(iface)
	local m = sm:read('canopen2.' .. iface)
	if not m then
		return false
	else		
		return true, m.nodes
	end
end

function get_status_class(is_active)
	if is_active then
		return "green"
	else
		return "red"
	end
end


function get_status_text(is_active)
	if is_active then
		return "online"
	else
		return "offline"
	end
end

function printNodesTable(nodes) 
	print('<table class="list" cellspacing="0" width="100%">')
	print('<tr>')
	print('<th>ID</th>')
	print('<th>Name</th>')
	print('<th>Status</th>')
	-- print('<th>Last seen</th>')
	print('<th>Hardware Version</th>')
	print('<th>Software Version</th>')
	print('<th>Error register</th>')
	print('</tr>')
	
	for i=0,#nodes do
		local n = nodes[i]
		if (n.is_active or n.last_seen ~= 0) then
			local id = i + 1
	
			-- print('<pre>')
			-- print(table.concat(row, "\t"))
			-- print('</pre>')
			
			print('<tr style="text-align: center;">')
			print('<td>' .. id .. '</td>')
			print('<td>' .. n.name .. '</td>')
			print('<td style="color: ' .. get_status_class(n.is_active) .. '">' .. get_status_text(n.is_active) .. '</td>')
			-- print('<td>' .. (n.last_seen) .. '</td>')
			print('<td>' .. n.hw_version .. '</td>')
			print('<td>' .. n.sw_version .. '</td>')
			print('<td>' .. n.error_register .. '</td>')
			print('</tr>')
		end
	end
	print('</table>')
end


-- -----------
-- Main

local result, nodes = readMemory('can0')  
if (result == true) then
	print('<h3>Instance 1</h3>')
	printNodesTable(nodes)
else
	print('<div class="error">Could not read shared memory ' .. 'canopen2.can0</div>')
end

result, nodes = readMemory('can1')  
if (result == true) then
	print('<h3>Instance 2</h3>')
	printNodesTable(nodes)
else
	-- no error if can1 does not exist
end


-- Print help
printHelp([[
<p>This is a list of all nodes as read from shared memory in the Pluto system.</p>

<p>More functionality for CANOpen 2 comes with the ui-canopen package and will be added to tab CANOpen 2 GUI.</p>
]])

print([[
<style>
    table tr:nth-child(odd) {
        background-color: #EBEFF3;
    }
    table tr:nth-child(even) {
        background-color: white;
    }
    label {
        width: 80px;
        
    }​
</style>
]])