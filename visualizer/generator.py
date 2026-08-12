import struct
import numpy as np
import cv2
from numba import njit, prange

@njit
def check_line_of_sight(x0, y0, x1, y1, grid_solid_1d, width):
    dx = x1 - x0
    dy = y1 - y0
    
    if dx == 0 and dy == 0:
        return not grid_solid_1d[y0 * width + x0]
        
    x, y = x0, y0
    stepX = 1 if dx > 0 else (-1 if dx < 0 else 0)
    stepY = 1 if dy > 0 else (-1 if dy < 0 else 0)
    
    tDeltaX = abs(1.0 / dx) if dx != 0 else np.inf
    tDeltaY = abs(1.0 / dy) if dy != 0 else np.inf
    
    tMaxX = tDeltaX * 0.5 if dx != 0 else np.inf
    tMaxY = tDeltaY * 0.5 if dy != 0 else np.inf
    
    if grid_solid_1d[y * width + x]: return False
    
    while x != x1 or y != y1:
        if tMaxX < tMaxY - 1e-6:
            tMaxX += tDeltaX
            x += stepX
        elif tMaxY < tMaxX - 1e-6:
            tMaxY += tDeltaY
            y += stepY
        else:
            if grid_solid_1d[y * width + (x + stepX)]: return False
            if grid_solid_1d[(y + stepY) * width + x]: return False
            tMaxX += tDeltaX
            tMaxY += tDeltaY
            x += stepX
            y += stepY
            
        if grid_solid_1d[y * width + x]: return False
        
    return True

def build_quadtree(grid_2d, x, y, size, max_size=2048):
    nodes = []
    
    def subdivide(cx, cy, csize):
        subgrid = grid_2d[cy:cy+csize, cx:cx+csize]
        is_all_solid = np.all(subgrid)
        is_all_empty = not np.any(subgrid)
        
        limit = 2048 if is_all_solid else max_size
        
        if csize <= limit and (is_all_solid or is_all_empty or csize == 1):
            is_solid = bool(subgrid[0,0])
            nodes.append((cx, cy, csize, csize, is_solid))
        else:
            half = csize // 2
            subdivide(cx, cy, half)
            subdivide(cx + half, cy, half)
            subdivide(cx, cy + half, half)
            subdivide(cx + half, cy + half, half)
            
    subdivide(x, y, size)
    return nodes

@njit
def get_node_test_points(x, y, w, h):
    pts = np.zeros((5, 2), dtype=np.int32)

    pts[0, 0] = x + w // 2; pts[0, 1] = y + h // 2
    pts[1, 0] = x; pts[1, 1] = y
    pts[2, 0] = x + w - 1; pts[2, 1] = y
    pts[3, 0] = x; pts[3, 1] = y + h - 1
    pts[4, 0] = x + w - 1; pts[4, 1] = y + h - 1
    return pts

@njit(parallel=True)
def compute_visibility_quadtree(nodes_np, grid_solid_1d, width, padding):
    num_nodes = len(nodes_np)
    vis_matrix = np.zeros(num_nodes * num_nodes, dtype=np.bool_)
    
    for obs_idx in prange(num_nodes):
        if nodes_np[obs_idx, 4] == 1:
            continue
            
        obs_pts = get_node_test_points(nodes_np[obs_idx, 0], nodes_np[obs_idx, 1], nodes_np[obs_idx, 2], nodes_np[obs_idx, 3])
        
        for tgt_idx in range(obs_idx + 1, num_nodes):
            if nodes_np[tgt_idx, 4] == 1:
                continue
                
            tgt_pts = get_node_test_points(nodes_np[tgt_idx, 0], nodes_np[tgt_idx, 1], nodes_np[tgt_idx, 2], nodes_np[tgt_idx, 3])
            
            can_see = False
            for o in range(5):
                for t in range(5):
                    if check_line_of_sight(obs_pts[o, 0], obs_pts[o, 1], tgt_pts[t, 0], tgt_pts[t, 1], grid_solid_1d, width):
                        can_see = True
                        break
                if can_see:
                    break
                        
            if can_see:
                vis_matrix[obs_idx * num_nodes + tgt_idx] = True
                vis_matrix[tgt_idx * num_nodes + obs_idx] = True
                
    if padding > 0:
        padded_matrix = np.zeros_like(vis_matrix)
        for obs_idx in prange(num_nodes):
            if nodes_np[obs_idx, 4] == 1:
                continue
            
            for tgt_idx in range(num_nodes):
                if vis_matrix[obs_idx * num_nodes + tgt_idx]:

                    tgt_x = nodes_np[tgt_idx, 0]
                    tgt_y = nodes_np[tgt_idx, 1]
                    tgt_w = nodes_np[tgt_idx, 2]
                    tgt_h = nodes_np[tgt_idx, 3]
                    
                    pad_min_x = tgt_x - padding
                    pad_max_x = tgt_x + tgt_w + padding
                    pad_min_y = tgt_y - padding
                    pad_max_y = tgt_y + tgt_h + padding
                    
                    for n_idx in range(num_nodes):
                        if nodes_np[n_idx, 4] == 0:
                            nx = nodes_np[n_idx, 0]
                            ny = nodes_np[n_idx, 1]
                            nw = nodes_np[n_idx, 2]
                            nh = nodes_np[n_idx, 3]
                            
                            if (nx < pad_max_x and nx + nw > pad_min_x and
                                ny < pad_max_y and ny + nh > pad_min_y):
                                padded_matrix[obs_idx * num_nodes + n_idx] = True
        return padded_matrix
        
    return vis_matrix

@njit(parallel=True)
def pack_bool_to_bytes(vis_matrix, num_bytes):
    payload = np.zeros(num_bytes, dtype=np.uint8)
    for i in prange(num_bytes):
        val = 0
        for bit in range(8):
            idx = i * 8 + bit
            if idx < len(vis_matrix) and vis_matrix[idx]:
                val |= (1 << bit)
        payload[i] = val
    return payload

def process_image(img_path, threshold, size):
    img = cv2.imread(img_path, cv2.IMREAD_UNCHANGED)
    if img is None:
        raise ValueError(f"Could not load image at {img_path}")
    if img.shape[2] != 4:
        raise ValueError("Image does not have an alpha channel (must be RGBA).")
        
    alpha = img[:, :, 3]
    solid_mask = (alpha < threshold).astype(np.uint8)
    resized = cv2.resize(solid_mask, (size, size), interpolation=cv2.INTER_NEAREST)
    return (resized > 0)

def export_bin(out_path, width, height, rx, ry, rs, nodes_np, vis_matrix):
    num_nodes = len(nodes_np)
    total_bits = num_nodes * num_nodes
    num_bytes = (total_bits + 7) // 8
    
    payload = pack_bool_to_bytes(vis_matrix, num_bytes)
    
    header = struct.pack("<4sIIIfffI", b"SAWH", 2, width, height, rx, ry, rs, num_nodes)
    
    node_bytes = bytearray()
    for n in nodes_np:
        node_bytes.extend(struct.pack("<HHHHB", n[0], n[1], n[2], n[3], n[4]))
        
    with open(out_path, "wb") as f:
        f.write(header)
        f.write(node_bytes)
        f.write(payload.tobytes())

