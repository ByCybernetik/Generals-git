#!/usr/bin/env python3
"""Case aliases for VC6-era headers on case-sensitive filesystems (Linux)."""
from pathlib import Path
import sys

# Lowercase/mixed on disk -> extra include spellings used by Generals / VC6.
VC6_HEADER_ALIASES: dict[str, list[str]] = {
    'matrix3d.h': ['Matrix3D.h', 'Matrix3d.h'],
    'vector3.h': ['Vector3.h'],
    'vector2.h': ['Vector2.h'],
    'vector4.h': ['Vector4.h'],
    'wwmath.h': ['WWMath.h'],
    'rect.h': ['Rect.h'],
    'plane.h': ['Plane.h'],
    'tri.h': ['Tri.h'],
    'coltype.h': ['ColType.h', 'Coltype.h'],
    # WW3D2 — VC6 include spellings (disk names are lowercase)
    'light.h': ['Light.h'],
    'texture.h': ['Texture.h'],
    'scene.h': ['Scene.h'],
    'line3d.h': ['Line3D.h'],
    'rendobj.h': ['RendObj.h'],
    'matinfo.h': ['MatInfo.h'],
    'hlod.h': ['HLod.h', 'HLOD.h'],
    'hanim.h': ['HAnim.h'],
    'part_emt.h': ['Part_Emt.h', 'Part_emt.h'],
    'htree.h': ['HTree.h'],
    'animobj.h': ['AnimObj.h'],
    'camera.h': ['Camera.h'],
    'dx8renderer.h': ['DX8Renderer.h'],
    'mesh.h': ['Mesh.h'],
    'meshmdl.h': ['MeshMdl.h', 'Meshmdl.h'],
    'segline.h': ['Segline.h'],
    'ww3d.h': ['WW3D.h'],
    'predlod.h': ['PredLod.h'],
    'part_ldr.h': ['Part_Ldr.h'],
    'dx8caps.h': ['DX8Caps.h'],
    'ww3dformat.h': ['WW3DFormat.h'],
    'render2dsentence.h': ['Render2DSentence.h'],
    'sortingrenderer.h': ['SortingRenderer.h'],
    'textureloader.h': ['Textureloader.h', 'TextureLoader.h'],
    'dx8webbrowser.h': ['DX8WebBrowser.h', 'dx8WebBrowser.h'],
    'meshmatdesc.h': ['Meshmatdesc.h', 'MeshMatDesc.h'],
    'rddesc.h': ['Rddesc.h'],
    'colorspace.h': ['Colorspace.h'],
    'pointgr.h': ['PointGr.h'],
    'streak.h': ['Streak.h'],
    'rinfo.h': ['RInfo.h'],
    'coltest.h': ['Coltest.h', 'ColTest.h'],
    'render2d.h': ['Render2D.h'],
    'assetmgr.h': ['AssetMgr.h'],
    'dx8wrapper.h': ['DX8Wrapper.h'],
    'dx8indexbuffer.h': ['DX8IndexBuffer.h'],
    'dx8vertexbuffer.h': ['DX8VertexBuffer.h'],
    'vertmaterial.h': ['VertMaterial.h'],
    'shader.h': ['Shader.h'],
    'texproject.h': ['Texproject.h'],
    'surfaceclass.h': ['SurfaceClass.h'],
    'lightenvironment.h': ['LightEnvironment.h'],
    'bittype.h': ['BitType.h'],
}

# Directory name aliases under WWVegas (WWMATH/... includes).
VC6_DIR_ALIASES: dict[str, list[str]] = {
    'WWMath': ['WWMATH', 'wwmath'],
    'WWLib': ['WWLIB', 'wwlib'],
    'WW3D2': ['ww3d2'],
}

# Includes that do not match simple first-letter-uppercase rules.
MANUAL_HEADER_ALIASES: dict[tuple[str, str], list[str]] = {
    ('Code/GameEngine/Include/Common', 'STLTypedefs.h'): ['STLTypeDefs.h'],
    ('Code/GameEngine/Include/Common', 'crc.h'): ['CRC.h'],
    ('Code/GameEngine/Include/Common', 'XferCRC.h'): ['XFerCRC.h'],
    ('Code/GameEngine/Include/Common', 'SubsystemInterface.h'): ['SubSystemInterface.h'],
    ('Code/GameEngine/Include/GameLogic/Module', 'DieModule.h'): ['Diemodule.h'],
    ('Code/Libraries/Include/Lib', 'BaseType.h'): ['basetype.h'],
    ('Code/GameEngine/Include/GameNetwork', 'networkutil.h'): ['NetworkUtil.h'],
    ('Code/GameEngine/Include/GameClient', 'GUICallbacks.h'): ['GuiCallbacks.h'],
    ('Code/GameEngine/Include/GameClient', 'GadgetListBox.h'): ['GadgetListbox.h'],
    ('Code/Libraries/Source/WWVegas/WWDownload', 'downloaddefs.h'): ['downloadDefs.h'],
    ('Code/Libraries/Source/WWVegas/WWDownload', 'Download.h'): ['download.h'],
    ('Code/GameEngine/Include/GameNetwork/GameSpy', 'PeerDefs.h'): ['peerDefs.h'],
    ('Code/Libraries/Source/WWVegas/WWLib', 'refcount.h'): ['RefCount.h'],
    ('Code/GameEngineDevice/Include/W3DDevice/GameClient', 'W3DVideobuffer.h'): ['W3DVideoBuffer.h'],
    ('Code/Main', 'resource.h'): ['Resource.h'],
}

# Lowercase directory aliases for VC6 includes (lib/foo.h, common/bar.h).
DIR_CASE_ALIASES: list[tuple[str, str, str]] = [
    ('Code/Libraries/Include', 'Lib', 'lib'),
    ('Code/GameEngine/Include', 'Common', 'common'),
    ('Code/GameEngine/Include', 'GameClient', 'Gameclient'),
    ('Code/GameEngine/Include', 'GameLogic', 'Gamelogic'),
    ('Code/GameEngineDevice/Include', 'Win32Device', 'Win32DEvice'),
]


def _ensure_symlink(link: Path, target: str) -> bool:
    if link.exists():
        return False
    link.symlink_to(target)
    return True


def _capitalized_basename(name: str) -> str | None:
    if not name.endswith('.h'):
        return None
    stem = name[:-2]
    if not stem or stem[0].isupper():
        return None
    return stem[0].upper() + stem[1:]


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    dirs = [
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'WWLib',
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'WWDebug',
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'WWMath',
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'Wwutil',
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'WW3D2',
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'WWSaveLoad',
        root / 'Code' / 'Libraries' / 'Source' / 'WWVegas' / 'WWDownload',
        root / 'Code' / 'Libraries' / 'Include' / 'Lib',
        root / 'Code' / 'GameEngine' / 'Include',
        root / 'Code' / 'GameEngineDevice' / 'Include',
        root / 'Code' / 'Main',
    ]
    created = 0

    wwvegas = root / 'Code' / 'Libraries' / 'Source' / 'WWVegas'
    for real_name, aliases in VC6_DIR_ALIASES.items():
        real_dir = wwvegas / real_name
        if not real_dir.is_dir():
            continue
        for alias in aliases:
            if _ensure_symlink(wwvegas / alias, real_name):
                created += 1

    for rel_parent, real_dir_name, alias_dir_name in DIR_CASE_ALIASES:
        parent = root / rel_parent
        real_dir = parent / real_dir_name
        if real_dir.is_dir():
            if _ensure_symlink(parent / alias_dir_name, real_dir_name):
                created += 1

    for d in dirs:
        if not d.is_dir():
            continue
        for path in d.rglob('*.h'):
            if not path.is_file():
                continue
            parent_rel = str(path.parent.relative_to(root)).replace('\\', '/')
            real_name = path.name

            cap = _capitalized_basename(real_name)
            if cap and cap + '.h' != real_name:
                if _ensure_symlink(path.parent / (cap + '.h'), real_name):
                    created += 1

            low = real_name.lower()
            if low != real_name:
                if _ensure_symlink(path.parent / low, real_name):
                    created += 1

            for alias in VC6_HEADER_ALIASES.get(real_name.lower(), []):
                if _ensure_symlink(path.parent / alias, real_name):
                    created += 1

            for alias in MANUAL_HEADER_ALIASES.get((parent_rel, real_name), []):
                if _ensure_symlink(path.parent / alias, real_name):
                    created += 1

    print(f'gen_case_symlinks: {created} aliases')
    return 0


if __name__ == '__main__':
    sys.exit(main())
