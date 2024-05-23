#16900
shaman miracle~
0 k 100
~
if (%self.hitp% < %self.maxhitp% / 4)
  if (%num% < 5)
    %echo% The num is %self.mana%.
    %echo% A heavenly angel descends and miraculously heals %self.name% !
    dg_cast 'miracle' %self%
    eval num %num% + 1
    global num
  end
end
~
$~
