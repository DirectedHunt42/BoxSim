import sys
import math
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QToolBar, QGraphicsView, QGraphicsScene,
    QGraphicsRectItem, QPushButton, QSlider, QLineEdit, QLabel
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
        self.forces = {}  # Dict to store active forces for drawing, e.g., {'Gravity': QPointF, ...}
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

    def reset_forces(self):
        self.acceleration = QPointF(0, 0)
        self.forces = {}

    def apply_force(self, force, name):
        self.acceleration += force / self.mass_
        if name in self.forces:
            self.forces[name] += force
        else:
            self.forces[name] = force

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
        return super().itemChange(change, value)

    def paint(self, painter, option, widget):
        super().paint(painter, option, widget)
        painter.drawText(self.boundingRect(), Qt.AlignCenter, self.label_)
        if BoxItem.show_forces:
            for name, force in self.forces.items():
                self.draw_force_vector(painter, force, name)

    def is_near_corner(self, pos):
        r = self.rect()
        return abs(pos.x() - r.width()) < 10 and abs(pos.y() - r.height()) < 10

    def is_near_rotate_handle(self, pos):
        r = self.rect()
        return abs(pos.x() - r.width() / 2) < 10 and pos.y() < -10

    def snap_rotation(self, angle):
        # TODO: Snap to nearby boxes - for now, 15 degrees
        return round(angle / 15) * 15

    def draw_force_vector(self, painter, force, label):
        magnitude = math.hypot(force.x(), force.y())
        if magnitude == 0:
            return

        center = self.boundingRect().center()
        direction = force / magnitude
        length = max(12.0, min(120.0, magnitude * 0.15))
        end = center + direction * length

        if label == "Gravity":
            color = Qt.blue
        elif "Normal" in label:
            color = Qt.darkGreen
        elif "Contact" in label:
            color = Qt.red
        else:
            color = Qt.darkYellow

        pen = QPen(color, 2)
        painter.setPen(pen)
        painter.drawLine(center, end)

        angle = math.atan2(direction.y(), direction.x())
        arrow_size = max(6.0, min(16.0, length * 0.2))
        arrow1 = end - QPointF(arrow_size * math.cos(angle - math.pi / 6), arrow_size * math.sin(angle - math.pi / 6))
        arrow2 = end - QPointF(arrow_size * math.cos(angle + math.pi / 6), arrow_size * math.sin(angle + math.pi / 6))
        painter.drawLine(end, arrow1)
        painter.drawLine(end, arrow2)
        painter.drawText(end, label)
        painter.setPen(QPen(Qt.black))

    # Helper to get world corners for collision math
    def get_corners(self):
        rect = self.rect()
        transform = self.sceneTransform()
        corners = [
            transform.map(rect.topLeft()),
            transform.map(rect.topRight()),
            transform.map(rect.bottomRight()),
            transform.map(rect.bottomLeft())
        ]
        return corners

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

        toolbar.addWidget(QLabel("Gravity"))
        gravity_slider = QSlider(Qt.Horizontal)
        gravity_slider.setRange(0, 200)
        gravity_slider.setValue(98)
        toolbar.addWidget(gravity_slider)
        self.gravity_slider = gravity_slider

        self.gravity_edit = QLineEdit("9.8")
        self.gravity_edit.setFixedWidth(50)
        toolbar.addWidget(self.gravity_edit)

        gravity_slider.valueChanged.connect(self.update_gravity_from_slider)
        self.gravity_edit.textChanged.connect(self.update_gravity_from_edit)

        toolbar.addWidget(QLabel("Dampen"))
        self.dampen_slider = QSlider(Qt.Horizontal)
        self.dampen_slider.setRange(0, 100)
        self.dampen_slider.setValue(50)
        self.dampen_slider.setToolTip("Collision dampening: 0 = very bouncy, 100 = no bounce")
        toolbar.addWidget(self.dampen_slider)

        self.scene = QGraphicsScene(self)
        self.view = QGraphicsView(self.scene, self)
        self.setCentralWidget(self.view)
        self.update_scene_bounds()

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.simulate_step)
        self.timer.setInterval(16)

        self.next_label_index = 0
        self.boxes = []
        self.damping = self.dampen_slider.value() / 100.0
        self.dampen_slider.valueChanged.connect(self.update_damping)

    def update_scene_bounds(self):
        viewport_rect = self.view.viewport().rect()
        self.scene.setSceneRect(0, 0, viewport_rect.width(), viewport_rect.height())

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.update_scene_bounds()

    def update_damping(self, val):
        self.damping = val / 100.0

    def effective_restitution(self):
        return max(0.0, min(1.0, 1.0 - self.damping))

    def update_gravity_from_slider(self, val):
        g = val / 10.0
        self.gravity_edit.setText(str(g))
        BoxItem.gravity = g

    def update_gravity_from_edit(self, text):
        try:
            g = float(text)
            self.gravity_slider.setValue(int(g * 10))
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

        box = BoxItem(label, 1.0, 20, 20, 50, 50)
        self.scene.addItem(box)
        self.boxes.append(box)

    def toggle_forces(self):
        BoxItem.show_forces = not BoxItem.show_forces
        self.scene.update()

    def simulate_step(self):
        dt = 0.016
        restitution = self.effective_restitution()
        bounds = self.scene.sceneRect()
        left_x = bounds.left()
        right_x = bounds.right()
        top_y = bounds.top()
        bottom_y = bounds.bottom()

        # Reset forces/accelerations
        for box in self.boxes:
            box.reset_forces()

        # Apply gravity to all
        for box in self.boxes:
            gravity_force = QPointF(0, box.mass_ * BoxItem.gravity)
            box.apply_force(gravity_force, "Gravity")

        # Handle scene boundary collisions (left/right walls, ceiling, floor)
        for box in self.boxes:
            corners = box.get_corners()
            min_x = min(p.x() for p in corners)
            max_x = max(p.x() for p in corners)
            min_y = min(p.y() for p in corners)
            max_y = max(p.y() for p in corners)

            floor_penetration = max_y - bottom_y
            if floor_penetration > 0:
                rel_vel = box.velocity.y()
                if rel_vel > 0:
                    impulse = -(1 + restitution) * rel_vel * box.mass_
                    box.velocity += QPointF(0, impulse / box.mass_)
                box.apply_force(QPointF(0, -box.mass_ * BoxItem.gravity), "Floor Normal")
                box.setPos(box.pos() + QPointF(0, -floor_penetration))

            ceiling_penetration = top_y - min_y
            if ceiling_penetration > 0:
                rel_vel = box.velocity.y()
                if rel_vel < 0:
                    impulse = -(1 + restitution) * rel_vel * box.mass_
                    box.velocity += QPointF(0, impulse / box.mass_)
                box.apply_force(QPointF(0, box.mass_ * BoxItem.gravity), "Ceiling Normal")
                box.setPos(box.pos() + QPointF(0, ceiling_penetration))

            left_penetration = left_x - min_x
            if left_penetration > 0:
                rel_vel = box.velocity.x()
                if rel_vel < 0:
                    impulse = -(1 + restitution) * rel_vel * box.mass_
                    box.velocity += QPointF(impulse / box.mass_, 0)
                box.apply_force(QPointF(box.mass_ * BoxItem.gravity, 0), "Left Wall Normal")
                box.setPos(box.pos() + QPointF(left_penetration, 0))

            right_penetration = max_x - right_x
            if right_penetration > 0:
                rel_vel = box.velocity.x()
                if rel_vel > 0:
                    impulse = -(1 + restitution) * rel_vel * box.mass_
                    box.velocity += QPointF(impulse / box.mass_, 0)
                box.apply_force(QPointF(-box.mass_ * BoxItem.gravity, 0), "Right Wall Normal")
                box.setPos(box.pos() + QPointF(-right_penetration, 0))

        # Handle box-box collisions
        for i in range(len(self.boxes)):
            for j in range(i + 1, len(self.boxes)):
                box1 = self.boxes[i]
                box2 = self.boxes[j]
                if box1.collidesWithItem(box2):
                    # Simple: assume centers, compute normal as line between centers
                    delta = box2.pos() - box1.pos()
                    dist = math.hypot(delta.x(), delta.y())
                    if dist == 0:
                        continue
                    normal = delta / dist
                    # Relative velocity along normal
                    rel_vel = box2.velocity - box1.velocity
                    vel_along_normal = rel_vel.x() * normal.x() + rel_vel.y() * normal.y()
                    if vel_along_normal > 0:
                        continue  # Separating
                    # Impulse scalar
                    reduced_mass = 1 / (1 / box1.mass_ + 1 / box2.mass_)
                    impulse_scalar = -(1 + restitution) * vel_along_normal * reduced_mass
                    impulse = normal * impulse_scalar
                    # Apply
                    box1.velocity -= impulse / box1.mass_
                    box2.velocity += impulse / box2.mass_
                    # Separate (simple overlap approx)
                    overlap = 10  # Arbitrary, better to compute penetration
                    box1.setPos(box1.pos() - normal * (overlap / 2))
                    box2.setPos(box2.pos() + normal * (overlap / 2))
                    # For display, approx contact force as impulse / dt
                    force1 = -impulse / dt
                    force2 = impulse / dt
                    box1.apply_force(force1, f"Contact {box2.label_}")
                    box2.apply_force(force2, f"Contact {box1.label_}")

        # Integrate motion
        for box in self.boxes:
            new_vel = box.velocity + box.acceleration * dt
            pos = box.pos() + new_vel * dt
            box.velocity = new_vel
            box.setPos(pos)

        self.scene.update()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
