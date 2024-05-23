#6200
Magic User~
0 k 100
~
eval cast %random.20%
set actor %random.char%
if (%cast% > 15)
    dg_cast 'disruption' %actor%
elseif (%cast% > 10)
    dg_cast 'eldritch blast' %actor%
elseif (%cast% > 0)
    dg_cast 'magic missile' %actor%   
end
~
#6201
Elstar Nova & Summons~
0 k 50
~
eval rand %random.10%
if %self.fighting%
  if (%rand% > 8)
    %echo% 	YElstar makes a magical hand gesture and a mutant thing appears!'	n
    %load% mob 6214
  elseif (%rand% > 6)
    %echo% 	YElstar says, 'Witness my overwhelming power!'	n
    dg_cast 'nova'
  elseif (%rand% > 4)
    %echo% 	YElstar says, 'This one is going to burn!'	n      
    dg_cast 'fireblast'
  elseif (%rand% > 2)
    %echo% 	YElstar says, 'This one is going to sting!'	n
    dg_cast 'missile spray'      
  end
end
~
#6202
Magic User Debuffs~
0 k 100
~
if (%self.fighting%)
  dg_cast 'betray' %actor.name%
  dg_cast 'blind' %actor.name%
  dg_cast 'curse' %actor.name%
  dg_cast 'slow' %actor.name%
  dg_cast 'wither' %actor.name% 
  dg_cast 'paralyze' %actor.name%
end
~
#6203
Mutant Orc Guard - 6214~
0 b 100
~
if !%self.fighting%
  set actor %random.char%
  if %actor.is_pc%
    if %actor.fighting%
      emote screams 'I'll save you, Master Elstar!'
      kill %actor.name%
    end
  end
end
~
$~
