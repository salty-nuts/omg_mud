#1000
Ymir Gives Equipment~
0 g 100
~
* By Salty
if %actor.is_pc% && %actor.level% <= 100
  wait 2 sec
  if !%actor.eq(*)%
    say get some clothes on! Here, I will help.
    %load% obj 1000 %actor% light
    %load% obj 1001 %actor% rfinger
    %load% obj 1001 %actor% lfinger
    %load% obj 1002 %actor% neck1
    %load% obj 1002 %actor% neck2
    %load% obj 1003 %actor% body
    %load% obj 1004 %actor% head
    %load% obj 1005 %actor% feet
    %load% obj 1006 %actor% legs
    %load% obj 1007 %actor% hands
    %load% obj 1008 %actor% arms
    %load% obj 1013 %actor% shield
    %load% obj 1009 %actor% about
    %load% obj 1010 %actor% waist
    %load% obj 1011 %actor% rwrist
    %load% obj 1011 %actor% lwrist
    %load% obj 1012 %actor% wield
    %load% obj 1014 %actor% face
    %load% obj 1017 %actor% floating
    %load% obj 1016 %actor% inv
    %load% obj 1012 %actor% inv
    %load% obj 1015 %actor% inv
    %load% obj 1015 %actor% inv
    halt
  end
end
~
#1001
Ymir Greeting~
0 e 0
* By Salty
has entered the game.~
wait 2 sec
say Welcome to OMG MUD, %actor.name%!
wait 2 sec
if %actor.is_pc% && %actor.level% <= 100
  wait 1 sec
  if !%actor.eq(*)%
    say I have bestowed equipment upon you to aid in your journey.
    %load% obj 1000 %actor% light
    %load% obj 1001 %actor% rfinger
    %load% obj 1001 %actor% lfinger
    %load% obj 1002 %actor% neck1
    %load% obj 1002 %actor% neck2
    %load% obj 1003 %actor% body
    %load% obj 1004 %actor% head
    %load% obj 1005 %actor% feet
    %load% obj 1006 %actor% legs
    %load% obj 1007 %actor% hands
    %load% obj 1008 %actor% arms
    %load% obj 1013 %actor% shield
    %load% obj 1009 %actor% about
    %load% obj 1010 %actor% waist
    %load% obj 1011 %actor% rwrist
    %load% obj 1011 %actor% lwrist
    %load% obj 1012 %actor% wield
    %load% obj 1014 %actor% face
    %load% obj 1017 %actor% floating
    %load% obj 1016 %actor% inv
    %load% obj 1012 %actor% inv
    %load% obj 1015 %actor% inv
    %load% obj 1015 %actor% inv
  end
end
if !%actor.has_item(3006)%
  %load% obj 3006 %actor% inv
end
wait 3 sec
%zoneecho% 3001 A booming voice announces, 'Welcome %actor.name% to the realm!'
~
#1002
Beginning Teleport~
2 c 100
teleport~
if %actor.is_pc%
  switch %arg.car%
    case chessboard
      %teleport% %actor% 3600
      %force% %actor% look
      break
    case tower
      %teleport% %actor% 2200
      %force% %actor% look
      break
    case anthill
      %teleport% %actor% 4698
      %force% %actor% look
      break
    case nuclear
      %teleport% %actor% 1800
      %force% %actor% look
      break
    case dwarven
      %teleport% %actor% 6500
      %force% %actor% look
      break
    case default
      %send% %actor% You can teleport to the following destinations:  tower, nuclear, or chess.
      break
  done
end
~
#1003
Obelisk of Teleportation~
1 b 100
~
set actor %random.char%
if %actor.is_pc%
  switch %random.7%
    case 1
      %send% %actor% The Obelisk of Teleportation wants to teleport you somewhere.
    break
    case 2
      %send% %actor% The Obelisk of Teleportation can be used by typing 'teleport <destination>'.
    break
    case 3
      %send% %actor% The Obelisk of Teleportation has the following low-level destinations:  chessboard
    break
    case 4
      %send% %actor% The Obelisk of Teleportation wants to teleport mid-level destinations:  anthill, tower
    break
    case 5
      %send% %actor% The Obelisk of Teleportation wants to teleport high-level destinations:  nuclear, dwarven
    break
    case 6
      %send% %actor% The Obelisk of Teleportation is a one way device, you must recall to return.
    break
    case 7
      %send% %actor% The Obelisk of Teleportation is the way primary way zones can be accessed.
    break
    default
      %send% %actor% Help Obelisk for more information.  Uses Trigger #1300
    break
  done
end
~
#1004
Obelisk of Teleportation~
1 b 100
~
set actor %random.char%
if %actor.is_pc%
  switch %random.7%
    case 1
      %send% %actor% The Obelisk of Teleportation wants to teleport you somewhere.
    break
    case 2
      %send% %actor% The Obelisk of Teleportation can be used by typing 'teleport <destination>'.
    break
    case 3
      %send% %actor% The Obelisk of Teleportation has the following low-level destinations:  chessboard
    break
    case 4
      %send% %actor% The Obelisk of Teleportation wants to teleport mid-level destinations:  anthill, tower
    break
    case 5
      %send% %actor% The Obelisk of Teleportation wants to teleport high-level destinations:  nuclear, dwarven
    break
    case 6
      %send% %actor% The Obelisk of Teleportation is a one way device, you must recall to return.
    break
    case 7
      %send% %actor% The Obelisk of Teleportation is the way primary way zones can be accessed.
    break
    default
      %send% %actor% Help Obelisk for more information.  Uses Trigger #1300
    break
  done
end
~
#1005
floating recall ability~
1 c 7
re~
* By Salty
if %cmd% == recall
  eval teleporter_return_room %actor.room.vnum%
  remote  teleporter_return_room %actor.id%
  %send% %actor% You recall back to The Beginning.
  %echoaround% %actor% %actor.name% recalls back to The Beginning.
  %teleport% %actor% 1000
  %force% %actor% look
  %echoaround% %actor% %actor.name% appears back in The Beginning.
else
  return 0
end
~
#1006
Mob Restore~
0 k 100
~
eval dam %self.maxhitp%
%damage% %self% -%dam%
~
$~
