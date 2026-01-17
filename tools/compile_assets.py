import os
import subprocess
import sys
import hashlib

def get_hash(file_path):
    hasher = hashlib.md5()
    with open(file_path, 'rb') as f:
        buf = f.read()
        hasher.update(buf)
    return hasher.hexdigest()

def compile_assets(input_dir, output_dir, compiler_path):
    if not os.path.exists(compiler_path):
        print(f"Error: Compiler not found at {compiler_path}")
        return

    supported_extensions = {
        '.obj': '.tmodel',
        '.fbx': '.tmodel',
        '.gltf': '.tmodel',
        '.glb': '.tmodel',
        '.png': '.ttex',
        '.jpg': '.ttex',
        '.jpeg': '.ttex',
        '.tga': '.ttex',
        '.bmp': '.ttex'
    }

    for root, dirs, files in os.walk(input_dir):
        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext in supported_extensions:
                src_path = os.path.join(root, file)
                rel_path = os.path.relpath(src_path, input_dir)
                dst_path = os.path.join(output_dir, os.path.splitext(rel_path)[0] + supported_extensions[ext])
                
                os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                
                # Simple time-stamp check
                need_compile = True
                if os.path.exists(dst_path):
                    if os.path.getmtime(dst_path) >= os.path.getmtime(src_path):
                        need_compile = False
                
                if need_compile:
                    print(f"Compiling: {rel_path} -> {os.path.relpath(dst_path, output_dir)}")
                    try:
                        subprocess.run([compiler_path, src_path, dst_path], check=True)
                    except subprocess.CalledProcessError as e:
                        print(f"Failed to compile {src_path}")

if __name__ == "__main__":
    # Default paths relative to script location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(script_dir)
    
    # Assuming standard build structure
    compiler = os.path.join(root_dir, "build", "tools", "asset_compiler", "tinygl_ac.exe")
    if not os.path.exists(compiler):
        # Try without .exe for linux/mac
        compiler = os.path.join(root_dir, "build", "tools", "asset_compiler", "tinygl_ac")

    input_assets = os.path.join(root_dir, "tests", "assets")
    output_assets = os.path.join(root_dir, "build", "tests", "assets")

    compile_assets(input_assets, output_assets, compiler)
