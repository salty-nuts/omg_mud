#6500
Magic User - 6502, 09, 16~
0 k 50
~
eval cast %random.20%
if (%cast% > 15)
    dg_cast 'nova'
elseif (%cast% > 10)
    dg_cast 'fireblast'
elseif (%cast% > 5)
    dg_cast 'missile spray'
end
~
#6501
Cityguard - 6500~
0 b 50
~
if !%self.fighting%
  set actor %random.char%
  if %actor%
    if %actor.is_killer%
      emote screams 'HEY!!!  You're one of those PLAYER KILLERS!!!!!!'
      kill %actor.name%
    elseif %actor.is_thief%
      emote screams 'HEY!!!  You're one of those PLAYER THIEVES!!!!!!'
      kill %actor.name%
    elseif %actor.cha% < 6
      %send% %actor% %self.name% spits in your face.
      %echoaround% %actor% %self.name% spits in %actor.name%'s face.
    end
    if %actor.fighting%
      eval victim %actor.fighting%
      if %actor.align% < %victim.align% && %victim.align% >= 0
        emote screams 'PROTECT THE INNOCENT!  BANZAI!  CHARGE!  ARARARAGGGHH!'
        kill %actor.name%
      end
    end
  end
end
~
#6502
shopkeeper~
0 c 100
*~
if %cmd.mudcommand% == list
  tell %actor.name% I only sell to dwarves!
elseif %cmd.mudcommand% == buy
  tell %actor.name% I only buy from dwarves!
else
  return 0
end
~
#6503
Healer~
0 k 66
~
eval cast %random.100%
if (%cast% > 99)
  dg_cast 'hand of god' %self.name%
elseif (%cast% > 75)
  dg_cast 'miracle' %self.name%
elseif (%cast% > 50)
  dg_cast 'powerheal' %self.name%
elseif (%cast% > 25)
  dg_cast 'heal'    %self.name% 
else
  dg_cast 'nightmare' %actor.fighting%
end
~
#6504
Doctor Healer~
0 k 50
~
eval number 10
if (%self.hitp% < %self.maxhitp% / 3)
  eval heal (%self.maxhitp%-%self.hitp%)
  if (%num% < %number%)
    %echo% 
    %echo% @Y%self.name% runs to the shelf where %self.heshe% grabs some medical supplies!@n
    %echo% @Y%self.name% applies first aid and heals %self.himher%self!@n
    %echo% 
    %damage% %self% -%heal%
    eval num %num% + 1
    global num
  end
end
~
#6505
Wraith Caster~
0 k 75
~
eval rand %random.20%
if %self.fighting%
  set inroom %self.room%  
  set person %inroom.people%
  while (%person%)
    if %person.is_pc%
      if (%rand% > 15)
        dg_cast 'betrayal' %person.name%
      elseif (%rand% > 12)
        dg_cast 'wither' %person.name%
      elseif (%rand% > 9)
        dg_cast 'slow' %person.name%
      elseif (%rand% > 6)
        dg_cast 'paralyze' %person.name%
      elseif (%rand% > 3)
        dg_cast 'curse' %person.name%
      else
        dg_cast 'blind' %person.name%
      end
    set person %person.next_in_room%  
  done
end
~
#6506
Taskmaster Calls Help~
0 k 25
~
if %self.fighting%
  %zoneecho% %self.room.vnum% @Y%self.name% shouts, 'INTRUDERS!  GUARDS, ASSEMBLE ON ME!'@n
  %load% mob 6518
end
~
$~
