#33001
Portal to Rock~
2 c 100
cl~
if %cmd% == climb && rock /= %arg%
  %send% %actor% You climb atop the rock.
  %echoaround% %actor% %actor.name% slowly climbs to the top of the rock.
  %teleport% %actor% 33019
  %force% %actor% look
  %echoaround% %actor% %actor.name% joins you atop the rock.
else
  %send% %actor% %cmd% climb what?!
end
~
#33002
Portal to Rock~
2 c 100
cl~
if %cmd% == climb && down /= %arg%
  %send% %actor% You climb down from the rock.
  %echoaround% %actor% %actor.name% slowly climbs down from on top of the rock.
  %teleport% %actor% 33008
  %force% %actor% look
  %echoaround% %actor% %actor.name% arrives from atop the rock.
else
  %send% %actor% %cmd% climb what?!
end
~
$~
