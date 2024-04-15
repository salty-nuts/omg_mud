#2100
wizard mana regen~
1 b 100
~
eval actor %self.worn_by%
if (%actor.mana% < %actor.maxmana%)
  nop %actor.mana(1000)%
  if (%actor.hitp% > 5000)
    %send% %actor% Your health is consumed and returned as mana.
    %damage% %actor% 2000
  end
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
if %person.is_pc%
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
eval actor %self.worn_by%
if (%actor.mana% < %actor.maxmana%)
  nop %actor.mana(1000)%
  %send% %actor% The Mark of Life restores your mana, use it wisely.
end
~
$~
