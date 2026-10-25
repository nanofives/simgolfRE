from port.test import lib as L, vectors as V
lib=L.load(); vr=V.VectorRunner(lib)
fs=["appendPairTrackPoint","addGolferPair","resetGolfer","golferAge","typeBit7Clear","appendEndearment","appendWalkerName",
"spawnObject","spawnWalker","addHabitatSlot","tickAnimals","placeHabitat","updateWalkers","initGolferState","addGolferToTable","loadGolfPro"]
tot=0
for d in (1,0):
  lib.defect("loadGolfPro.negativeWindowBound",d)
  for f in fs:
    r=vr.run_function(f); print(d,f,r.summary()); tot+=0
