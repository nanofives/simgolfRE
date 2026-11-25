"""Render a scene over a base and save art | shapes for given windows (frame coords) at given zoom."""
import os
import numpy as np
import common as C, deliver as D


def windows(art_full, base_full, scene, wins, z, name, origin=(0, 0)):
    tiles = []
    for (x, y, w, h) in wins:
        a = art_full[y - origin[1]:y - origin[1] + h, x - origin[0]:x - origin[0] + w]
        b = base_full[y - origin[1]:y - origin[1] + h, x - origin[0]:x - origin[0] + w]
        tiles.append(D.tile(a, b, scene, (x, y), z))
    C.save_png(C.stack(tiles), os.path.join(C.OUT, 'read', name))
