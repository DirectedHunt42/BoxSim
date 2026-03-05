import sys
import math
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QToolBar, QGraphicsView, QGraphicsScene,
    QGraphicsRectItem, QPushButton, QSlider, QLineEdit
)
from PyQt5.QtCore import QTimer, QPointF, Qt, QRectF
from PyQt5.QtGui import QBrush, QPen, QPainter

class BoxItem(QGraphicsRectItem):
    simulating = False
    show_forces = False
    gravity = 9.81

    def __init__(self, label, mass, x, y, w, h):
        super().__init__(x, y, w, h)
        self.label_ = label
        self.mass_ = mass
        self.velocity = QPointF(0, 0)
        self.acceleration = QPointF(0, 0)
        self.setFlag(QGraphicsRectItem.ItemIsMovable, True)
        self.setFlag(QGraphicsRectItem.ItemIsSelectable, True)
        self.setFlag(QGraphicsRectItem.ItemSendsGeometryChanges, True)
        self.setAcceptHoverEvents(True)
        self.setBrush(QBrush(Qt.lightGray))
        self.setPen(QPen(Qt.black))
        self.fixed_rotation = 0
        self.setRotation(self.fixed_rotation)
        self.resize_mode = False
        self.rotate_mode = False

    def mass(self):
        return self.mass_

    def set_mass(self, mass):
        self.mass_ = mass

    def apply_force(self, force):
        self.acceleration += force / self.mass_

    def hoverEnterEvent(self, event):
        super().hoverEnterEvent(event)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton and not BoxItem.simulating:
            pos = event.pos()
            self.resize_mode = False
            self.rotate_mode = False
            if self.is_near_corner(pos):
                self.resize_mode = True
                self.start_pos = event.scenePos()
                self.start_rect = self.rect()
            elif self.is_near_rotate_handle(pos):
                self.rotate_mode = True
                self.start_angle = math.atan2(pos.y(), pos.x())
            else:
                super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self.resize_mode:
            delta = event.scenePos() - self.start_pos
            new_rect = QRectF(self.start_rect)
            new_rect.setWidth(new_rect.width() + delta.x())
            new_rect.setHeight(new_rect.height() + delta.y())
            self.setRect(new_rect.normalized())
        elif self.rotate_mode:
            pos = event.pos()
            angle = math.atan2(pos.y(), pos.x()) - self.start_angle
            new_rotation = self.fixed_rotation + math.degrees(angle)
            snapped = self.snap_rotation(new_rotation)
            self.setRotation(snapped)
            self.fixed_rotation = snapped
        else:
            super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        self.resize_mode = False
        self.rotate_mode = False
        super().mouseReleaseEvent(event)

    def itemChange(self, change, value):
        if change == QGraphicsRectItem.ItemPositionChange and BoxItem.simulating:
            return self.pos()
        return super().itemChange(change, value)

    def paint(self, painter, option, widget):
        super().paint(painter, option, widget)
        painter.drawText(self.boundingRect(), Qt.AlignCenter, self.label_)
        if BoxItem.show_forces:
            self.draw_force_vector(painter, self.gravity_force(), "Gravity")
            self.draw_force_vector(painter, self.normal_force(), "Normal")

    def is_near_corner(self, pos):
        r = self.rect()
        return abs(pos.x() - r.width()) < 10 and abs(pos.y() - r.height()) < 10

    def is_near_rotate_handle(self, pos):
        r = self.rect()
        return abs(pos.x() - r.width() / 2) < 10 and pos.y() < -10

    def snap_rotation(self, angle):
        return round(angle / 15) * 15

    def gravity_force(self):
        return QPointF(0, self.mass_ * BoxItem.gravity)

    def normal_force(self):
        return QPointF(0, -self.mass_ * BoxItem.gravity)

    def draw_force_vector(self, painter, force, label):
        center = self.boundingRect().center()
        scale = 10.0
        end = center + force / scale
        painter.drawLine(center, end)
        angle = math.atan2(force.y(), force.x())
        arrow1 = end - QPointF(10 * math.cos(angle - math.pi / 6), 10 * math.sin(angle - math.pi / 6))
        arrow2 = end - QPointF(10 * math.cos(angle + math.pi / 6), 10 * math.sin(angle + math.pi / 6))
        painter.drawLine(end, arrow1)
        painter.drawLine(end, arrow2)
        painter.drawText(end, label)

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Forces and Free Body Diagrams Demo")
        self.resize(800, 600)

        toolbar = self.addToolBar("Controls")

        self.play_pause = QPushButton("Play")
        toolbar.addWidget(self.play_pause)
        self.play_pause.clicked.connect(self.toggle_simulation)

        reset = QPushButton("Reset")
        toolbar.addWidget(reset)
        reset.clicked.connect(self.reset_scene)

        add_box = QPushButton("Add Box")
        toolbar.addWidget(add_box)
        add_box.clicked.connect(self.add_new_box)

        toggle_forces = QPushButton("Show Forces")
        toolbar.addWidget(toggle_forces)
        toggle_forces.clicked.connect(self.toggle_forces)

        gravity_slider = QSlider(Qt.Horizontal)
        gravity_slider.setRange(0, 200)
        gravity_slider.setValue(98)
        toolbar.addWidget(gravity_slider)

        self.gravity_edit = QLineEdit("9.8")
        toolbar.addWidget(self.gravity_edit)

        gravity_slider.valueChanged.connect(self.update_gravity_from_slider)
        self.gravity_edit.textChanged.connect(self.update_gravity_from_edit)

        self.scene = QGraphicsScene(self)
        self.view = QGraphicsView(self.scene, self)
        self.setCentralWidget(self.view)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.simulate_step)
        self.timer.setInterval(16)

        self.next_label_index = 0
        self.boxes = []

    def update_gravity_from_slider(self, val):
        g = val / 10.0
        self.gravity_edit.setText(str(g))
        BoxItem.gravity = g

    def update_gravity_from_edit(self, text):
        try:
            g = float(text)
            self.findChild(QSlider).setValue(int(g * 10))
            BoxItem.gravity = g
        except ValueError:
            pass

    def toggle_simulation(self):
        BoxItem.simulating = not BoxItem.simulating
        if BoxItem.simulating:
            self.timer.start()
            self.play_pause.setText("Pause")
        else:
            self.timer.stop()
            self.play_pause.setText("Play")
        for box in self.boxes:
            box.setFlag(QGraphicsRectItem.ItemIsMovable, not BoxItem.simulating)

    def reset_scene(self):
        self.scene.clear()
        self.boxes = []
        self.next_label_index = 0

    def add_new_box(self):
        if self.next_label_index < 26:
            label = chr(ord('A') + self.next_label_index)
        else:
            label = chr(ord('A') + (self.next_label_index % 26)) + str(self.next_label_index // 26)
        self.next_label_index += 1

        box = BoxItem(label, 1.0, 0, 0, 50, 50)
        self.scene.addItem(box)
        self.boxes.append(box)

    def toggle_forces(self):
        BoxItem.show_forces = not BoxItem.show_forces
        self.scene.update()

    def simulate_step(self):
        dt = 0.016

        for box in self.boxes:
            box.acceleration = QPointF(0, 0)

        for box in self.boxes:
            box.apply_force(QPointF(0, box.mass_ * BoxItem.gravity))

        for i in range(len(self.boxes)):
            for j in range(i + 1, len(self.boxes)):
                if self.boxes[i].collidesWithItem(self.boxes[j]):
                    delta = self.boxes[j].pos() - self.boxes[i].pos()
                    dist = math.hypot(delta.x(), delta.y())
                    if dist > 0:
                        normal = delta / dist
                        force_mag = 100.0
                        force = normal * force_mag
                        self.boxes[i].apply_force(-force)
                        self.boxes[j].apply_force(force)

            if self.boxes[i].pos().y() + self.boxes[i].rect().height() > 500:
                self.boxes[i].apply_force(QPointF(0, -BoxItem.gravity * self.boxes[i].mass_))
                if self.boxes[i].velocity.y() > 0:
                    self.boxes[i].velocity = QPointF(self.boxes[i].velocity.x(), 0)

        for box in self.boxes:
            vel = box.velocity + box.acceleration * dt
            pos = box.pos() + vel * dt
            box.velocity = vel
            box.setPos(pos)

        self.scene.update()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())