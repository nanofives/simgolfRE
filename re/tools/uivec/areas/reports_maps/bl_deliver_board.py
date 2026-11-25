import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bl_board as B, common as C, deliver as D

g, st = B.load()
rgb, m = C.sheet("buy_land.pcx")
outer = {"t": "poly", "rings": [B.diamond(*g["outer"])]}
base_full = C.wipe(rgb, outer)
sc = B.scene(g, st)
x, y, w, h = 30, 255, 740, 345
det = [D.tile(rgb[b:b + hh, a:a + ww], base_full[b:b + hh, a:a + ww], sc, (a, b), 5)
       for a, b, ww, hh in ((24, 390, 110, 100), (340, 250, 120, 70), (670, 390, 110, 100))]
D.deliver("buyland-board", [("idle", rgb[y:y + h, x:x + w], base_full[y:y + h, x:x + w], sc, (x, y))],
          "an iso (2:1) diamond frame and board: frame band between two diamonds (outer centre (399.6, 438.8) half 365.3 x 182.75, "
          "the black diamond (400, 439.5) 312 x 156, edges read as exact 2:1 lines), lit by a measured bevel by edge direction "
          "(pale on edges facing left / up-left, a dark band on those facing down-right) with a soft shadow on the marble; the "
          "black band; 3 x 3 lots of 192 x 96 (cream corners and ends, tan sides, black centre). The marbled ball behind stays art",
          unsure="the art's lot and black edges are hard pixel staircases; the shapes are antialiased", extra_tiles=det, z=2)
