#6702
Missing Persons - Corporal~
0 d 1
missing~
if %actor.is_pc%
 wait 2 seconds
 say What do you care about the missing gnomes? I only do what the Chieftain orders...
 wait 2 seconds
 say Now get the hell out of my sight, nosey S O B!
end
~
#6733
Climb Ladder Up~
2 c 100
cl~
if %cmd% == climb && ladder /= %arg%
  %send% %actor% You climb the ladder out of the crypt.
  %echoaround% %actor% %actor.name% climbs the ladder out of the crypt.
  %teleport% %actor% 6734
  %force% %actor% look
  %echoaround% %actor% %actor.name% has arrived from the crypt below.
else
  %send% %actor% %cmd% climb what?!
end
~
#6734
Climb Ladder Down~
2 c 100
cl~
if %cmd% == climb && ladder /= %arg%
  %send% %actor% You slowly climb down into the crypt.
  %echoaround% %actor% %actor.name% slowly climbs into the crypt below.
  %teleport% %actor% 6733
  %force% %actor% look
  %echoaround% %actor% %actor.name% has arrived from above.
else
  %send% %actor% %cmd% climb what?!
end
~
#6746
Reveal Behind Altar~
0 f 100
~
%send% %actor% The altar slides away from the wall as Seriphus crashes down upon it, revealing a secret room!
%echoaround% %actor% The altar slides away from the wall as Seriphus crashes down upon it, revealing a secret room!
%door% 6746 west room 6747
set has_opened_altar 1
global has_opened_altar
~
#6747
Purge Altar Exit~
2 f 100
~
%door% 6746 west purge 0
~
#6748
Priest Greet~
0 g 100
~
if %actor.is_pc%
 wait 1 seconds
 say Hello, it's nice to see some new faces. Fear not, you are safe in this temple!
 wait 2 seconds
 say Have you seen or heard anything about the missing gnomes?
end
~
#6749
Missing Person - Priest~
0 d 1
missing~
if %actor.is_pc%
 wait 2 seconds
 say Unfortunately, many gnomes have mysteriously vanished lately.
 wait 2 seconds
 say The most notable to vanish were Fezzy Fizzelbaum and Quibby Quazzelbaum.
 wait 2 seconds
 say Coincidentally it coincides with when the Chieftain ordered the village to worship a new god...
 wait 2 seconds
 say What he doesn't understand is Gahim lives inside our hearts, not in this temple.
 wait 2 seconds
 say Corporal Lanhim has been acting a little strange lately. Could you go talk to him and help me get to the bottom of all of this? If you bring me evidence of the culprit's demise I will reward you...
end
~
#6750
Give Priest Wings~
0 j 100
~
if %object.vnum% == 6706
 %purge% %object%
 say Oh thank you for solving this mystery!
 wait 2 seconds
 switch %random.3%
   case 1
     %echo% Quoben begins speaking in tongues to Seriphus' wings!
     wait 1 seconds
     %echo% The wings begin to flap violently and then fall lifeless as a black aura shoots from them with a deafening shriek!
     wait 2 seconds
     %load% obj 6707
     give wings %actor.name%
   break
   case 2
     %echo% Quoben begins speaking in tongues to Seriphus' wings!
     wait 1 seconds
     %send% %actor% Evil laughter booms from the wings as they fly out of Quoben's hands and into yours!
     %echoaround% %actor% Evil laughter booms from the wings as they fly out of Quoben's hands and into %actor.name%'s hands!
    wait 2 seconds
   %load% obj 6708 %actor% inv
   break
   default
     %echo% Quoben begins speaking in tongues to Seriphus' wings!
     wait 2 seconds
     %echo% The wings began flapping violently as they fly out of Quoben's hands and vanish in a puff of black smoke!
     wait 2 seconds
     %load% obj 6713
     give staff %actor.name%
     wait 1 seconds
     say May Gahim bless you on your journey, brave adventurer!
   break
 done
 elseif %object.vnum% == 6721
  %send% %actor% You give a note to Quoben, the Priest of Gahim.
  %echoaround% %actor% %actor.name% gives Quoben, the Priest of Gahim a note.
  %purge% %object%
  switch %random.10%
    case 1
     say I feel confident we can soon reach peace, please take this for all your help!
     wait 1 seconds
     %load% obj 6724
     give medallion %actor.name%
    break
    default
     wait 1 seconds
     say I feel confident we can soon reach peace, please take this for all your help!
     %load% obj 6723
     give bag %actor.name%
    break
  done
  elseif %object.vnum% != 6721 || 6706
   say Not sure I have a use for that, keep it.
   return 0
end
~
#6751
Give Priest Note~
0 j 100
~
if %object.vnum% == 6721
 %send% %actor% You a note to Quoben, the Priest of Gahim.
 %echoaround% %actor% %actor.name% gives Quoben, the Priest of Gahim a note.
 %purge% %object%
 switch %random.10%
   case 1
    say I feel confident we can soon reach peace, please take this for all your help!
    wait 1 seconds
    %load% obj 6724
    give medallion %actor.name%
   break
   default
    wait 1 seconds
    say I feel confident we can soon reach peace, please take this for all your help!
    %load% obj 6723
    give bag %actor.name%
   break
 done
 elseif %object.vnum% != 6719
  say Not sure I have a use for that, keep it.
  return 0
end
~
#6767
Cayri entry~
0 g 100
~
wait 1 seconds
%echo% Princess Cayri sobs, tears flowing down her face. Perhaps you could inquire about what is wrong...
~
#6768
What's wrong, Cayri?~
0 d 100
wrong~
wait 1 sec
say I have tried everything to convince Daddy to negotiate a peace deal with the gnomes.
wait 2 sec
say But I recently found out the evil which has been plaguing the gnomes was caused by one of our own...
wait 2 sec
say Do you even care about my feelings, or the gnomes?
~
#6769
Cayri - ritualist~
0 d 100
yes~
wait 1 sec
say I recently discovered a hobgoblin has been communicating with demonic spirits through ancient rituals.
wait 2 sec
say He is responsible for the summoning of the demonic spirit which is plaguing the gnome village. I fear he will unleash one upon our kingdom as well.
wait 2 sec
say His powers are too much for us to defeat as he has grown very strong, take this key and save the gnomes and hobgoblins from destruction, please!
wait 2 sec
%load% obj 6797
give key %actor.name%
wait 1 sec
say Bring me his book of rituals so it can be destroyed!
~
#6770
Give Cayri guide~
0 j 100
~
if %object.vnum% == 6719
 %send% %actor% You give a guide to advanced rituals to Princess Cayri.
 %echoaround% %actor% %actor.name% gives Princess Cayri a guide to advanced rituals.
 %purge% %object%
 switch %random.10%
   case 1
    say You truly are courageous, please take this as a token of gratitude!
    wait 1 seconds
    %load% obj 6722
    give veil %actor.name%
    wait 2 seconds
    say Take this note back to Quoben and let him know of my discovery, please.
    %load% obj 6721
    give note %actor.name%
   break
   default
    wait 1 seconds
    say You truly are courageous, please take this as a token of gratitude!
    %load% obj 6723
    give bag %actor.name%
    wait 2 seconds
    say Take this note back to Quoben and let him know of my discovery, please.
    %load% obj 6721
    give note %actor.name%
   break
 done
 elseif %object.vnum% != 6719
  say Not sure I have a use for that, keep it.
  return 0
end
~
$~
