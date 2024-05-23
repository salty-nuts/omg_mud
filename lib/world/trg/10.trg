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
    %load% obj 1017 %actor% float
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
0 g 100
~
wait 1 sec
say Welcome back to The Beginning, %actor.name%.
wait 1 sec
if %actor.is_pc% && %actor.level% <= 100
  wait 1 sec
  if !%actor.eq(*)%
    say %actor.name%, I have bestowed equipment upon you to aid in your journey.
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
    %load% obj 1012 %actor% inv
    %load% obj 1014 %actor% face
    %load% obj 1017 %actor% float
    %load% obj 1016 %actor% inv
    %load% obj 1012 %actor% inv
    %load% obj 1015 %actor% inv
    %load% obj 1015 %actor% inv
  end
end
~
#1005
float recall ability~
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
#1008
test new char eq~
2 s 100
~
wait 1 sec
if %actor.is_pc% && %actor.level% <= 100
  if !%actor.eq(*)%
    %echo% %actor.name%, it does not appear you have any equipment.
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
    %load% obj 1012 %actor% inv
    %load% obj 1014 %actor% face
    %load% obj 1017 %actor% float
    %load% obj 1016 %actor% inv
    %load% obj 1012 %actor% inv
    %load% obj 1015 %actor% inv
    %load% obj 1015 %actor% inv
    wait 1 sec    
    %force% %actor% eq all
    wait 1 sec
    %force% %actor% inventory
    wait 1 sec
    %echo% %actor.name%, the gods have equipped you for exploration. 
    halt
  end
end
~
#1099
Ymir Shop~
0 c 100
*~
if %cmd.mudcommand% == list
  *
  %send% %actor%  ##   Available   Item                                 Cost in Gold Coins
  %send% %actor% -------------------------------------------------------------------------
  %send% %actor%   1)  Unlimited   Scroll of Identify                                 Free
  %send% %actor%   2)  Unlimited   Scroll of Recall                                   Free
  %send% %actor%   3)  Unlimited   Biscuit of Hard Tack                               Free
  %send% %actor%   4)  Unlimited   Waterskin                                          Free

  *
elseif %cmd.mudcommand% == buy
  if identify /= %arg% || %arg% == 1
    set item 1099
    %load% obj %item% %actor% inv
    tell %actor.name% here is your Scroll of Identify.
    nop %actor.gold(-%item_cost%)%
  end
  if recall /= %arg% || %arg% == 2
    set item 1098
    %load% obj %item% %actor% inv
    tell %actor.name% here is your Scroll of Recall.
    nop %actor.gold(-%item_cost%)%
  end
  if hard /= %arg% || tack /= %arg% || biscuit /= %arg% || %arg% == 3
    set item 1097
    %load% obj %item% %actor% inv
    tell %actor.name% here is your hard tack biscuit.
    nop %actor.gold(-%item_cost%)%
  end
  if water /= %arg% || skin /= %arg% || waterskin /= %arg% || %arg% == 4
    set item 1096
    %load% obj %item% %actor% inv
    tell %actor.name% here is your waterskin.
    nop %actor.gold(-%item_cost%)%
  end
elseif %cmd.mudcommand% == sell
  tell %actor.name% I don't want anything you have.
else
  return 0
end
~
$~
