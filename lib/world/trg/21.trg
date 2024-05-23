#2100
wizard mana regen~
1 b 100
~
eval person %self.worn_by%
if %person.room.roomflag(PEACEFUL)%
  halt
end
if (%person.mana% < %person.maxmana%) && (%person.level% < 101)
  nop %person.mana(500)%
  %send% %person% Some of your mana is restored by the Mark of Death so you may kill.
end
~
#2101
xolotl's block~
0 q 100
~
%send% %actor% Xolotl, the Dog-Headed God growls at you menacingly, and blocks your path.
%echoaround% %actor% Xolotl, the Dog-Headed God growls at %actor.name% menacingly, and blocks %actor.himher%!
return 0
~
#2102
obisian bracer damage~
1 b 100
~
eval person %self.worn_by%
if %person.room.roomflag(PEACEFUL)%
  halt
end
if %person.is_pc% && (%person.level% < 101)
  %send% %person% The spirit of Xolotl empowers you!
  dg_affect %person% damroll 5 1
end
~
#2103
divine intervention~
0 k 100
~
if (%self.hitp% < %self.maxhitp% / 4)
  if (%num% < 5)
    %echo% The num is %self.mana%.
    %echo% A heavenly angel descends and miraculously heals %self.name% !
    dg_cast 'divine intervention' %self%
    eval num %num% + 1
    global num
  end
end
~
#2104
priest mana regen~
1 b 100
~
eval person %self.worn_by%
if %person.room.roomflag(PEACEFUL)%
  halt
end
if (%person.mana% < %person.maxmana%)
  nop %person.mana(500)%
  %send% %person% The Mark of Life restores some mana, use it wisely.
end
~
#2105
npc heals to full~
0 k 100
~
eval number 3
if (%self.hitp% < %self.maxhitp% / 2)
  eval heal (%self.maxhitp%-%self.hitp%)
  if (%num% < %number%)
    %echo% 		n
    %echo% 		G%self.name% restores %self.himher%self to full health!		n
    %echo% 		n
    %damage% %self% -%heal%
    eval num %num% + 1
    global num
  end
end
~
#2106
shaman miracle~
0 k 100
~
if (%self.hitp% < %self.maxhitp% / 4)
  eval heal %self.maxhitp% - %self.hitp%
  if (%num% < 5)
    %echo% Huitzilopochtli descends and miraculously heals %self.name% !
    set %self.hitp(%heal%)
    eval num %num% + 1
    global num
  end
end
~
#2107
death touch~
0 k 100
~
eval victim %self.fighting%
eval hurt %victim.hitp% / 4
eval heal %self.hitp% * 2
if (%self.hitp% < %self.maxhitp% / 4)
  if (%num% < 5)
    %echo% 		n
    %echo% 		R%self.name% reaches out a decaying finger and touches %victim.name% !		n
    %echo% 		RYou are disgusted as %self.name% heals %self.himher%self with %victim.name%'s life force.
    %echo% 		n
    %damage% %victim% %hurt%
    %damage% %self% -%heal%
    eval num %num% + 1
    global num
  end
end
~
#2108
Mictlantecuhtli aura of death~
0 k 10
~
if %actor.fighting%
  set inroom %actor.room%
  set person %inroom.people%
  %echo% @RAn icy wind chills your very bones as shadowy tendrils appear from the underworld.@n
  while (%person%)
    if %person.is_pc%
      eval siphon (%person.hitp% / 10)
      eval heal %siphon%
      %send% %person% @R%self.name% siphons some of your life force!@n
      %damage% %person% %siphon%
      %damage% %self% -%heal%
    end
    set person %person.next_in_room%
  done
end
~
#2109
Mictlan Tendrils Damage Player~
2 b 100
~
set person %self.people%
while (%person%)
  set tmp_target %person.next_in_room%
  if %person.is_pc%
    eval siphon %person.hitp% / 10
    %send% %person% The souls of Mictlan siphon your life force!
    %echo% Trapped souls in Mictlan coalesce into dark tendrils and siphon life from %person.name%!
    %damage% %person% %siphon%    
  end
  set person %tmp_target%
done
~
#2110
Mictecacihuatl heals to full~
0 k 100
~
eval number 5
if (%self.hitp% < %self.maxhitp% / 2)
  eval heal (%self.maxhitp%-%self.hitp%)
  if (%num% < %number%)
    %echo% 	n
    %echo% 	R%self.name% restores %self.himher%self to full health!	n
    %echo% 	n
    %damage% %self% -%heal%
    eval num %num% + 1
    global num
  end
end
~
#2111
bard mana regen~
1 b 100
~
eval person %self.worn_by%
if %person.room.roomflag(PEACEFUL)%
  halt
end
if (%person.mana% < %person.maxmana%)
  nop %person.mana(500)%
  %send% %person% You are blessed as The Art of Song restores some mana.
end
~
#2112
sigil blood moon align~
1 b 100
~
eval person %self.worn_by%
eval align %person.align%
set evil -1000
set talign -500
if %align% > %talign%
  %send% %person% %self.shortdesc% draws you toward Mictlan, you feel evil.
  eval %person.align(-1000)%
end
~
$~
