"""Build the kart driver's animations: a seated driving pose for the ValveBiped skeleton.

    blender -b -P assets_src/kart_driver/build_kart_driver.py [-- --preview /tmp/kart_driver-renders]
    tools/wine/studiomdl.sh assets_src/kart_driver/generated/kart_driver.qc

Compiles to models/kart/driver_anims.mdl: an animation-only model (no mesh). The client (C_KartDriver) evaluates its
sequences and copies the bone rotations onto the driver's own player model, so the one rig serves the citizens
(male and female) and the combine.

The skeleton (bone names, parents and rest pose) is read at build time from the installed SDK's
models/player/male_anims.mdl, so nothing of Valve's is committed: the SMDs and the QC go to generated/, which is
gitignored. The pose is built from code in kart model space (x forward, y left, z up, 1 unit = 1 inch), around
models/kart/kart_racer.mdl:

- the pelvis on the seat, reclined against the seat back, the spine curving forward towards the wheel, the
  shoulders forward, the head level and looking ahead;
- the legs forward to the pedals with the knees slightly up, the feet tilted up onto them;
- the hands at ten to two: the palm (Anim_Attachment_LH/RH, where HL2MP holds weapons) on grip_l/grip_r, the
  fingers round the rim and the elbows out and down.

Sequences and pose parameters:

- kart_drive_idle: blends over `lean` (-1 full left .. +1 full right), the upper body leaning into the turn with the
  head kept nearer upright and the hands still on the grips. Its autolayer kart_head_yaw adds the head turn.
- kart_head_yaw: a delta layer over `head_yaw` (-60 .. 60 degrees, positive to the driver's left), turning the neck
  and head about the upper body's up axis.

--preview DIR renders the pose as a stick figure in the racer kart (front, side, 3q, leaning left, looking left) to
check it. Needs Blender Source Tools for the kart.
"""
import math
import os
import struct
import subprocess
import sys
import tempfile

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "generated")
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

SDK = os.environ.get("SDK2013_LINUX",
                     os.path.expanduser("~/.local/share/Steam/steamapps/common/Source SDK Base 2013 Multiplayer"))
SKELETON = "models/player/male_anims.mdl"  # in hl2mp/hl2mp_pak_dir.vpk

# The racer kart (assets_src/kart_racer/build_kart_racer.py): the steering wheel's centre and its tilt, with
# grip_l/grip_r on its rim at ten to two (wheel-local X to the top of the wheel, +Y to the driver's left).
STEERING_WHEEL = Matrix.Translation((9, 0, 31)) @ Matrix.Rotation(math.radians(-55), 4, "Y")
GRIPS = {s: STEERING_WHEEL @ Vector((7.5 * math.cos(math.radians(60)), s * 7.5 * math.sin(math.radians(60)), 0))
         for s in (1, -1)}

# The pose; SEAT and FEET match cl_kart_driver_seat and cl_kart_driver_feet.
SEAT = Vector((-7, 0, 25))          # the pelvis
FEET = Vector((24, 6, 12))          # the ankles; y mirrored for the right foot
RECLINE = 30.0                      # degrees the pelvis leans back against the seat back
SPINE_CURVE = (3.0, 4.0, 4.0, 3.0)  # degrees each spine bone bends forward again, Spine to Spine4
HEAD_PITCH = 4.0                    # degrees the face looks down from level
SHRUG_FORWARD = 14.0                # degrees the clavicles bring the shoulders forward
SHRUG_DOWN = 4.0
FOOT_PITCH = 35.0                   # degrees the feet point up onto the pedals
LEAN = 12.0                         # degrees the upper body leans into a full turn
LEAN_HEAD = 0.5                     # fraction of the lean the head takes back, staying nearer upright
HEAD_YAW = 60.0                     # degrees the head_yaw layer turns at either end
HEAD_YAW_NECK = 0.4                 # fraction of the turn the neck takes; the head takes the rest
CURL = {0: (15.0, 20.0, 15.0), 1: (45.0, 60.0, 40.0)}  # degrees per joint: the thumb, and the fingers

SPINE = ["Spine", "Spine1", "Spine2", "Spine4"]
B = "ValveBiped.Bip01_"

# Studiomdl turns the root bones 90 degrees about Z by default, like it did for Valve's animations: the SMDs are in
# that unturned space, facing -Y. Kart model space is the turned one, facing +X.
TO_MODEL = Matrix.Rotation(math.radians(90), 4, "Z")


# --- skeleton -------------------------------------------------------------------------------------------------

def extract(vpk, path, out_dir):
    """Extracts one file from a VPK with the SDK's vpk tool; returns its local path."""
    os.makedirs(os.path.join(out_dir, os.path.dirname(path)), exist_ok=True)
    env = dict(os.environ, LD_LIBRARY_PATH=os.path.join(SDK, "bin", "linux64"))
    subprocess.run([os.path.join(SDK, "bin", "linux64", "vpk"), "x", os.path.join(SDK, vpk), path],
                   cwd=out_dir, env=env, check=True, stdout=subprocess.DEVNULL)
    return os.path.join(out_dir, path)


def read_bones(mdl):
    """[(name, parent, pos, rot)] from an MDL's bone table (studiohdr_t v48, mstudiobone_t 216 bytes)."""
    with open(mdl, "rb") as f:
        data = f.read()
    count, offset = struct.unpack_from("<ii", data, 156)
    bones = []
    for i in range(count):
        o = offset + i * 216
        name_offset, parent = struct.unpack_from("<ii", data, o)
        name = data[o + name_offset:data.index(b"\0", o + name_offset)].decode()
        bones.append((name, parent, struct.unpack_from("<3f", data, o + 32), struct.unpack_from("<3f", data, o + 60)))
    return bones


def local_matrix(pos, rot):
    return Matrix.Translation(pos) @ Matrix.Rotation(rot[2], 4, "Z") @ Matrix.Rotation(rot[1], 4, "Y") \
        @ Matrix.Rotation(rot[0], 4, "X")


class Pose:
    """Bone-to-model matrices in kart model space, posed by rotating bones (with everything below them)."""

    def __init__(self, bones):
        self.bones = bones
        self.index = {b[0]: i for i, b in enumerate(bones)}
        self.children = [[] for _ in bones]
        self.world = []
        for i, (name, parent, pos, rot) in enumerate(bones):
            if parent >= 0:
                self.children[parent].append(i)
            self.world.append((self.world[parent] if parent >= 0 else TO_MODEL) @ local_matrix(pos, rot))

    def copy(self):
        other = Pose.__new__(Pose)
        other.bones, other.index, other.children = self.bones, self.index, self.children
        other.world = [m.copy() for m in self.world]
        return other

    def i(self, name):
        return self.index[name if name.startswith("ValveBiped.") else B + name]

    def head(self, name):
        return self.world[self.i(name)].translation.copy()

    def subtree(self, i):
        out = [i]
        for c in self.children[i]:
            out += self.subtree(c)
        return out

    def transform(self, name, m):
        for j in self.subtree(self.i(name)):
            self.world[j] = m @ self.world[j]

    def rotate(self, name, axis, degrees):
        """Turns a bone about a model-space axis through its head."""
        self.rotate_matrix(name, Matrix.Rotation(math.radians(degrees), 4, Vector(axis)))

    def rotate_matrix(self, name, rot):
        p = self.head(name)
        self.transform(name, Matrix.Translation(p) @ rot.to_4x4() @ Matrix.Translation(-p))

    def aim(self, name, tip, target):
        """Turns a bone the shortest way so the direction from its head to `tip` points along `target`."""
        here = tip - self.head(name)
        self.rotate_matrix(name, here.rotation_difference(target).to_matrix())

    def bend_local(self, name, degrees):
        """Turns a bone about its own local Z (how the fingers curl)."""
        axis = self.world[self.i(name)].to_3x3() @ Vector((0, 0, 1))
        self.rotate(name, axis, degrees)

    def set_rotation(self, name, rot3):
        m = self.world[self.i(name)]
        self.rotate_matrix(name, rot3 @ m.to_3x3().inverted())

    def ik(self, a, b, c, target, pole):
        """Two-bone IK: turns a and b so c's head reaches target, bending b towards pole."""
        pa, pb, pc = self.head(a), self.head(b), self.head(c)
        la, lb = (pb - pa).length, (pc - pb).length
        to = target - pa
        d = min(max(to.length, abs(la - lb) + 1e-3), la + lb - 1e-3)
        axis = to.normalized()
        side = (pole - axis * pole.dot(axis)).normalized()
        cos_a = (la * la + d * d - lb * lb) / (2 * la * d)
        joint = pa + (axis * cos_a + side * math.sqrt(max(0.0, 1 - cos_a * cos_a))) * la
        self.aim(a, pb, joint - pa)
        self.aim(b, self.head(c), target - joint)

    def smd_frame(self):
        """Local pos/rot per bone in SMD space."""
        lines = []
        for i, (name, parent, _, _) in enumerate(self.bones):
            parent_world = self.world[parent] if parent >= 0 else TO_MODEL
            local = parent_world.inverted() @ self.world[i]
            pos, rot = local.translation, local.to_euler("XYZ")
            lines.append("%d %.6f %.6f %.6f %.6f %.6f %.6f" % (i, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z))
        return lines


# --- the pose -------------------------------------------------------------------------------------------------

def place_hand(pose, side, target):
    """Hand `side` ("L"/"R") holding the rim at `target`: palm on it facing into the wheel, fingers over the outside
    of the rim, the thumb along it towards the top; the arm reaches with the elbow out and down."""
    s = 1 if side == "L" else -1
    centre = STEERING_WHEEL.translation
    column = STEERING_WHEEL.to_3x3() @ Vector((0, 0, 1))  # towards the driver
    outward = (target - centre - column * (target - centre).dot(column)).normalized()
    x = (outward - column * 0.35).normalized()  # to the fingers, over and round the rim
    y = (column - x * column.dot(x)).normalized()  # the back of the hand faces the driver; the palm is -Y
    z = x.cross(y)
    rot = Matrix((x, y, z)).transposed()
    hand = "%s_Hand" % side
    palm = pose.world[pose.i("ValveBiped.Anim_Attachment_%sH" % side)].translation - pose.head(hand)
    palm_local = pose.world[pose.i(hand)].to_3x3().inverted() @ palm
    wrist = target - rot @ palm_local
    pose.ik("%s_UpperArm" % side, "%s_Forearm" % side, hand, wrist, Vector((0, s * 0.6, -1)).normalized())
    pose.set_rotation(hand, rot)
    pose.transform(hand, Matrix.Translation(wrist - pose.head(hand)))


def curl_fingers(pose, side):
    for finger in range(5):
        angles = CURL[0 if finger == 0 else 1]
        for joint, degrees in enumerate(angles):
            name = "%s_Finger%d%s" % (side, finger, "" if joint == 0 else str(joint))
            if B + name in pose.index:
                pose.bend_local(name, -degrees)


def drive_pose(rest, lean=0.0):
    pose = rest.copy()
    left, forward, up = Vector((0, 1, 0)), Vector((1, 0, 0)), Vector((0, 0, 1))

    pose.transform("Pelvis", Matrix.Translation(SEAT - pose.head("Pelvis")))
    pose.rotate("Pelvis", left, -RECLINE)
    for bone, degrees in zip(SPINE, SPINE_CURVE):
        pose.rotate(bone, left, degrees)
    face = pose.world[pose.i("Head1")].to_3x3() @ Vector((0, -1, 0))  # the face is the head's local -Y
    down = math.degrees(math.atan2(-face.z, face.x))
    pose.rotate("Neck1", left, (HEAD_PITCH - down) * 0.5)
    pose.rotate("Head1", left, (HEAD_PITCH - down) * 0.5)

    # Into the turn: positive lean is right, a roll about forward; the head takes some of it back.
    for bone in SPINE:
        pose.rotate(bone, forward, LEAN * lean / len(SPINE))
    pose.rotate("Neck1", forward, -LEAN * lean * LEAN_HEAD)

    for side, s in (("L", 1), ("R", -1)):
        pose.rotate("%s_Clavicle" % side, up, -s * SHRUG_FORWARD)
        pose.rotate("%s_Clavicle" % side, forward, -s * SHRUG_DOWN)

        feet = Vector((FEET.x, s * FEET.y, FEET.z))
        pose.ik("%s_Thigh" % side, "%s_Calf" % side, "%s_Foot" % side, feet, Vector((0, s * 0.15, 1)).normalized())
        pitch = math.radians(FOOT_PITCH)
        pose.aim("%s_Foot" % side, pose.head("%s_Toe0" % side), Vector((math.cos(pitch), 0, math.sin(pitch))))

        place_hand(pose, side, GRIPS[s])
        curl_fingers(pose, side)
    return pose


def head_yaw_pose(pose, degrees):
    pose = pose.copy()
    up = pose.world[pose.i("Spine4")].to_3x3() @ Vector((1, 0, 0))  # the spine's bones run along their local X
    pose.rotate("Neck1", up, degrees * HEAD_YAW_NECK)
    pose.rotate("Head1", up, degrees * (1 - HEAD_YAW_NECK))
    return pose


# --- output ---------------------------------------------------------------------------------------------------

def write_smd(path, pose):
    lines = ["version 1", "nodes"]
    lines += ['%d "%s" %d' % (i, b[0], b[1]) for i, b in enumerate(pose.bones)]
    lines += ["end", "skeleton", "time 0"] + pose.smd_frame() + ["end", ""]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def write_qc(path, bones):
    lines = [
        "// Written by build_kart_driver.py; compile with tools/wine/studiomdl.sh assets_src/kart_driver/generated/kart_driver.qc",
        '$modelname "kart/driver_anims.mdl"',
        "",
        "// The skeleton, so the bones are kept with no mesh on them.",
    ]
    for name, parent, pos, rot in bones:
        # $definebone takes the rotation as pitch yaw roll in degrees: the bone's Y, Z and X.
        lines.append('$definebone "%s" "%s" %.6f %.6f %.6f %.6f %.6f %.6f 0 0 0 0 0 0' % (
            name, bones[parent][0] if parent >= 0 else "", pos[0], pos[1], pos[2],
            math.degrees(rot[1]), math.degrees(rot[2]), math.degrees(rot[0])))
    # With no mesh, studiomdl only flags the bones it gives a hitbox to, and the client's bone setup skips unflagged
    # bones: their rotations would come out uninitialized. A small hitbox on every bone flags them all.
    lines += ["", "// A hitbox on every bone, so studiomdl flags them all as used."]
    for name, parent, pos, rot in bones:
        lines.append('$hbox 0 "%s" -0.5 -0.5 -0.5 0.5 0.5 0.5' % name)
    lines += [
        "",
        "$poseparameter lean -1 1",
        "$poseparameter head_yaw -%g %g" % (HEAD_YAW, HEAD_YAW),
        "",
        '$animation a_head_neutral "drive.smd" fps 30',
        '$animation a_head_right "head_right.smd" fps 30 subtract a_head_neutral 0',
        '$animation a_head_center "drive.smd" fps 30 subtract a_head_neutral 0',
        '$animation a_head_left "head_left.smd" fps 30 subtract a_head_neutral 0',
        "$sequence kart_head_yaw { a_head_right a_head_center a_head_left blendwidth 3 blend head_yaw -%g %g delta hidden }"
        % (HEAD_YAW, HEAD_YAW),
        "",
        '$sequence kart_drive_idle { "lean_left.smd" "drive.smd" "lean_right.smd" blendwidth 3 blend lean -1 1 fps 30 loop '
        "addlayer kart_head_yaw }",
        "",
    ]
    with open(path, "w") as f:
        f.write("\n".join(lines))


# --- preview --------------------------------------------------------------------------------------------------

def preview(poses, out_dir):
    """Stick figures of the poses in the racer kart, rendered with Workbench."""
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="io_scene_valvesource")
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "OBJECT"
    scene.render.resolution_x, scene.render.resolution_y = 900, 700
    scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.8, 0.82, 0.85)

    kart = os.path.join(ROOT, "assets_src", "kart_racer")
    bpy.ops.import_scene.smd(files=[{"name": "kart_racer.smd"}], directory=kart + os.sep)
    for ob in scene.objects:
        if ob.type == "MESH":
            ob.color = (0.55, 0.6, 0.7, 1)

    cam_data = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    light = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    scene.collection.objects.link(light)

    def figure(pose):
        obs = []
        for i, (name, parent, _, _) in enumerate(pose.bones):
            if parent < 0 or "Anim_Attachment" in name or name.endswith(".forward"):
                continue
            a, b = pose.world[parent].translation, pose.world[i].translation
            if (b - a).length < 1e-3:
                continue
            thick = 0.35 if "Finger" in name else 1.1
            bpy.ops.mesh.primitive_cylinder_add(radius=thick, depth=(b - a).length, vertices=10,
                                                location=(a + b) / 2)
            ob = bpy.context.object
            ob.rotation_mode = "QUATERNION"
            ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(b - a)
            ob.color = (0.85, 0.45, 0.2, 1) if "_L_" in name else (0.25, 0.5, 0.85, 1) if "_R_" in name \
                else (0.9, 0.85, 0.3, 1)
            obs.append(ob)
        head = pose.world[pose.i("Head1")]
        bpy.ops.mesh.primitive_uv_sphere_add(radius=4.2, location=head @ Vector((4, -0.8, 0)))
        bpy.context.object.color = (0.9, 0.85, 0.3, 1)
        obs.append(bpy.context.object)
        # The nose shows where the head faces: the head's local -Y.
        bpy.ops.mesh.primitive_cone_add(radius1=1.2, depth=3, location=head @ Vector((4, -5.2, 0)))
        nose = bpy.context.object
        nose.rotation_mode = "QUATERNION"
        nose.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(head.to_3x3() @ Vector((0, -1, 0)))
        nose.color = (0.8, 0.1, 0.1, 1)
        obs.append(nose)
        return obs

    # Camera offsets from the driver's middle, in kart model space.
    views = {"front": ((150, 0, 30), (0, 0, 30)), "side": ((0, -150, 25), (0, 0, 30)),
             "3q": ((95, -95, 60), (0, 0, 30)), "top": ((20, 0, 150), (0, 0, 30)),
             "hands": ((-30, -25, 35), (10, 0, 33))}
    for shot, (pose, view) in poses.items():
        obs = figure(pose)
        loc, target = Vector(views[view][0]), Vector(views[view][1])
        loc += target
        cam.location = loc
        cam.rotation_mode = "QUATERNION"
        cam.rotation_quaternion = (target - loc).to_track_quat("-Z", "Y")
        light.rotation_euler = (0.6, 0.2, 0.8)
        scene.render.filepath = os.path.join(out_dir, shot + ".png")
        bpy.ops.render.render(write_still=True)
        for ob in obs:
            bpy.data.objects.remove(ob, do_unlink=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    with tempfile.TemporaryDirectory() as tmp:
        bones = read_bones(extract(os.path.join("hl2mp", "hl2mp_pak_dir.vpk"), SKELETON, tmp))
    rest = Pose(bones)
    drive = drive_pose(rest)
    poses = {
        "drive": drive,
        "lean_left": drive_pose(rest, -1.0),
        "lean_right": drive_pose(rest, 1.0),
        "head_left": head_yaw_pose(drive, HEAD_YAW),
        "head_right": head_yaw_pose(drive, -HEAD_YAW),
    }
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, ".gitignore"), "w") as f:
        f.write("# Generated from Valve's skeleton by build_kart_driver.py; not committed.\n*\n")
    for name, pose in poses.items():
        write_smd(os.path.join(OUT, name + ".smd"), pose)
    write_qc(os.path.join(OUT, "kart_driver.qc"), bones)

    for side, s in (("L", 1), ("R", -1)):
        palm = drive.world[drive.i("ValveBiped.Anim_Attachment_%sH" % side)].translation
        print("%s palm %s, grip %s, foot %s" % (side, tuple(round(v, 2) for v in palm),
                                                tuple(round(v, 2) for v in GRIPS[s]),
                                                tuple(round(v, 2) for v in drive.head("%s_Foot" % side))))
    print("wrote %d SMDs and kart_driver.qc to %s" % (len(poses), OUT))

    if "--preview" in argv:
        preview({"front": (drive, "front"), "side": (drive, "side"), "3q": (drive, "3q"), "hands": (drive, "hands"),
                 "lean_left": (poses["lean_left"], "front"), "head_left": (poses["head_left"], "top")},
                argv[argv.index("--preview") + 1])


main()
