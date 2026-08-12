import sys
import os
import time
import cv2
import numpy as np
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
                             QPushButton, QLabel, QSpinBox, QDoubleSpinBox, 
                             QFileDialog, QMessageBox, QGraphicsView, QGraphicsScene)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QImage, QPixmap, QColor, QPen, QPainter

import generator

class VisualizerGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SAWH - Visualizer")
        self.resize(1000, 700)
        
        self.img_path = None
        self.grid_solid_1d = None
        self.vis_matrix = None
        self.width = 32
        self.height = 32
        self.selected_idx = -1
        
        self.init_ui()
        
    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        main_layout = QHBoxLayout()
        central_widget.setLayout(main_layout)
        
        left_panel = QWidget()
        left_layout = QVBoxLayout()
        left_panel.setLayout(left_layout)
        left_panel.setFixedWidth(250)
        
        self.btn_load = QPushButton("Load Map Image (WebP/PNG)")
        self.btn_load.clicked.connect(self.load_image)
        left_layout.addWidget(self.btn_load)
        
        self.lbl_file = QLabel("No file selected.")
        self.lbl_file.setWordWrap(True)
        left_layout.addWidget(self.lbl_file)
        
        left_layout.addSpacing(20)
        left_layout.addWidget(QLabel("Grid Size (N x N) [Powers of 2]:"))
        from PyQt5.QtWidgets import QComboBox
        self.combo_size = QComboBox()
        self.combo_size.addItems(["16", "32", "64", "128", "256", "512", "1024", "2048", "4096"])
        self.combo_size.setCurrentIndex(2)
        left_layout.addWidget(self.combo_size)
        
        left_layout.addWidget(QLabel("Max Node Size (Power of 2):"))
        from PyQt5.QtWidgets import QComboBox
        self.combo_max_node = QComboBox()
        self.combo_max_node.addItems(["0", "2", "4", "8", "16", "32", "64", "128", "256", "2048 (Unlimited)"])
        self.combo_max_node.setCurrentIndex(0)
        left_layout.addWidget(self.combo_max_node)
        
        left_layout.addWidget(QLabel("Alpha Threshold:"))
        self.spin_thresh = QSpinBox()
        self.spin_thresh.setRange(0, 255)
        self.spin_thresh.setValue(50)
        left_layout.addWidget(self.spin_thresh)
        
        left_layout.addWidget(QLabel("Visibility Padding:"))
        self.spin_pad = QSpinBox()
        self.spin_pad.setRange(0, 10)
        self.spin_pad.setValue(1)
        left_layout.addWidget(self.spin_pad)
        
        left_layout.addSpacing(20)
        
        left_layout.addWidget(QLabel("Radar Pos X:"))
        self.spin_rx = QDoubleSpinBox()
        self.spin_rx.setRange(-10000, 10000)
        self.spin_rx.setValue(0.0)
        left_layout.addWidget(self.spin_rx)
        
        left_layout.addWidget(QLabel("Radar Pos Y:"))
        self.spin_ry = QDoubleSpinBox()
        self.spin_ry.setRange(-10000, 10000)
        self.spin_ry.setValue(0.0)
        left_layout.addWidget(self.spin_ry)
        
        left_layout.addWidget(QLabel("Radar Scale:"))
        self.spin_rs = QDoubleSpinBox()
        self.spin_rs.setRange(0.01, 100.0)
        self.spin_rs.setValue(1.0)
        left_layout.addWidget(self.spin_rs)
        
        left_layout.addStretch()
        
        self.btn_calc = QPushButton("REBUILD FROM IMAGE(Reset)")
        self.btn_calc.setStyleSheet("background-color: #3498DB; color: white; font-weight: bold; height: 40px;")
        self.btn_calc.clicked.connect(self.process_and_render)
        left_layout.addWidget(self.btn_calc)
        
        self.btn_vis = QPushButton("REBUILD FROM EDITS")
        self.btn_vis.setStyleSheet("background-color: #9B59B6; color: white; font-weight: bold; height: 40px;")
        self.btn_vis.clicked.connect(self.update_visibility_only)
        self.btn_vis.setEnabled(False)
        left_layout.addWidget(self.btn_vis)
        
        self.lbl_status = QLabel("Ready.")
        self.lbl_status.setWordWrap(True)
        left_layout.addWidget(self.lbl_status)
        
        self.btn_export = QPushButton("EXPORT MAP")
        self.btn_export.setStyleSheet("background-color: #2E8B57; color: white; font-weight: bold; height: 40px;")
        self.btn_export.clicked.connect(self.export_bin)
        self.btn_export.setEnabled(False)
        left_layout.addWidget(self.btn_export)
        
        main_layout.addWidget(left_panel)
        
        self.scene = QGraphicsScene()
        self.view = QGraphicsView(self.scene)
        self.view.mousePressEvent = self.canvas_clicked
        main_layout.addWidget(self.view)
        
    def load_image(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Open Map Image", "", "Images (*.png *.webp)")
        if file_path:
            self.img_path = file_path
            self.lbl_file.setText(os.path.basename(file_path))
            self.process_and_render()
        
    def process_and_render(self):
        if not self.img_path: return
        
        self.width = int(self.combo_size.currentText())
        self.height = int(self.combo_size.currentText())
        
        try:
            grid_2d = generator.process_image(self.img_path, self.spin_thresh.value(), self.width)
            self.grid_solid_1d = grid_2d.flatten()
            
            t_qt0 = time.time()
            max_node_str = self.combo_max_node.currentText()
            max_size = 1 if max_node_str == "0" or max_node_str == 0 else (2048 if "Unlimited" in str(max_node_str) else int(max_node_str))
            nodes_list = generator.build_quadtree(grid_2d, 0, 0, self.width, max_size)
            self.nodes_np = np.array(nodes_list, dtype=np.int32)
            t_qt1 = time.time()
            
            self.update_visibility_only()
            self.btn_vis.setEnabled(True)
            
        except Exception as e:
            QMessageBox.critical(self, "Error", str(e))
            self.lbl_status.setText("Error occurred.")
            
    def update_visibility_only(self):
        if self.nodes_np is None: return
        
        self.grid_solid_1d = np.zeros(self.width * self.height, dtype=np.bool_)
        for n in self.nodes_np:
            nx, ny, nw, nh, is_solid = n
            if is_solid:
                for y in range(ny, ny + nh):
                    for x in range(nx, nx + nw):
                        self.grid_solid_1d[y * self.width + x] = True
                        
        t0 = time.time()
        self.vis_matrix = generator.compute_visibility_quadtree(self.nodes_np, self.grid_solid_1d, self.width, self.spin_pad.value())
        t1 = time.time()
        
        self.lbl_status.setText(f"Raycast updated in {t1-t0:.3f}s. Map is ready to Export.")
        self.btn_export.setEnabled(True)
        self.render_canvas()
            
    def render_canvas(self):
        self.scene.clear()
        
        if self.nodes_np is None: return
        
        img_arr = np.zeros((self.height, self.width, 3), dtype=np.uint8)
        
        for i, n in enumerate(self.nodes_np):
            nx, ny, nw, nh, is_solid = n
            fill_color = [200, 50, 50] if is_solid else [80, 80, 80]
            cv2.rectangle(img_arr, (nx, ny), (nx + nw - 1, ny + nh - 1), fill_color, -1)
                    
        if self.selected_idx >= 0 and self.selected_idx < len(self.nodes_np):
            if self.nodes_np[self.selected_idx, 4] == 0:
                num_nodes = len(self.nodes_np)
                for tgt_idx in range(num_nodes):
                    if self.vis_matrix is not None and len(self.vis_matrix) > 0:
                        if self.vis_matrix[self.selected_idx * num_nodes + tgt_idx]:
                            tn = self.nodes_np[tgt_idx]
                            cv2.rectangle(img_arr, (tn[0], tn[1]), (tn[0]+tn[2]-1, tn[1]+tn[3]-1), [50, 200, 50], -1)
                
                sn = self.nodes_np[self.selected_idx]
                cv2.rectangle(img_arr, (sn[0], sn[1]), (sn[0]+sn[2]-1, sn[1]+sn[3]-1), [50, 100, 255], -1)
                
        h, w, c = img_arr.shape
        qimg = QImage(img_arr.data, w, h, w * c, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(qimg)
        
        scale_factor = 600 // max(self.width, self.height)
        if scale_factor < 1: scale_factor = 1
        pixmap = pixmap.scaled(self.width * scale_factor, self.height * scale_factor, Qt.KeepAspectRatio, Qt.FastTransformation)
        
        painter = QPainter(pixmap)
        for i, n in enumerate(self.nodes_np):
            nx, ny, nw, nh, is_solid = n
            border_color = QColor(150, 30, 30) if is_solid else QColor(60, 60, 60)
            painter.setPen(QPen(border_color, 1))
            painter.drawRect(nx * scale_factor, ny * scale_factor, nw * scale_factor - 1, nh * scale_factor - 1)
        painter.end()
        
        self.scene.addPixmap(pixmap)
        self.view.fitInView(self.scene.sceneRect(), Qt.KeepAspectRatio)
        self.pixmap_scale = scale_factor
        
    def canvas_clicked(self, event):
        QGraphicsView.mousePressEvent(self.view, event)
        
        if self.nodes_np is None: return
        
        scene_pos = self.view.mapToScene(event.pos())
        x_px = scene_pos.x()
        y_px = scene_pos.y()
        
        grid_x = int(x_px // self.pixmap_scale)
        grid_y = int(y_px // self.pixmap_scale)
        
        if 0 <= grid_x < self.width and 0 <= grid_y < self.height:
            clicked_idx = -1
            for i, n in enumerate(self.nodes_np):
                if n[0] <= grid_x < n[0] + n[2] and n[1] <= grid_y < n[1] + n[3]:
                    clicked_idx = i
                    break
                    
            if clicked_idx != -1:
                if event.button() == Qt.RightButton:
                    self.nodes_np[clicked_idx, 4] = 1 - self.nodes_np[clicked_idx, 4]
                    self.vis_matrix = np.zeros(0, dtype=np.bool_)
                    self.lbl_status.setText("Map manually edited! Click 'REBUILD FROM EDITS' to apply changes.")
                    self.btn_export.setEnabled(False)
                else:
                    self.selected_idx = clicked_idx
                    
            self.render_canvas()

    def export_bin(self):
        if self.vis_matrix is None: return
        
        out_path, _ = QFileDialog.getSaveFileName(self, "Save Bin File", "map.bin", "Bin Files (*.bin)")
        if out_path:
            try:
                generator.export_bin(
                    out_path, 
                    self.width, 
                    self.height, 
                    self.spin_rx.value(), 
                    self.spin_ry.value(), 
                    self.spin_rs.value(), 
                    self.nodes_np,
                    self.vis_matrix
                )
                QMessageBox.information(self, "Success", f"Successfully exported to {os.path.basename(out_path)}")
            except Exception as e:
                QMessageBox.critical(self, "Export Failed", str(e))

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = VisualizerGUI()
    window.show()
    sys.exit(app.exec_())
