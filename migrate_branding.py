import os
import re

# Configuration
ROOT_DIR = os.getcwd()

EXCLUDE_DIRS = {
    ".git",
    "build",
    "bin",
    "lib",
    "node_modules",
    "__pycache__",
    ".vscode",
    ".idea",
    "build-mingw", # Added common build dir
    "build-msvc"   # Added common build dir
}

EXCLUDE_EXTENSIONS = {
    ".exe", ".dll", ".obj", ".pdb", ".lib", ".cache", ".git", ".o", ".a", ".so", ".dylib", ".class", ".jar"
}

# Branding Rules
# Order matters! Specific replacements first.
REPLACEMENTS = [
    # 1. Error Codes: NPP1001 -> NR1001
    (r'NPP(\d{4})', r'NR\1'),

    # 2. File Extensions in text: .npp -> .nr
    # We use a negative lookbehind/lookahead to avoid messing up things if necessary,
    # but .npp is pretty unique.
    (r'\.npp', r'.nr'),

    # 3. Explicit variations of "Neuron++"
    (r'Neuron\+\+', r'Neuron'),
    (r'neuron\+\+', r'neuron'),
    (r'N\+\+', r'Neuron'),
    (r'n\+\+', r'neuron'),

    # 4. The raw acronym "NPP" -> "Neuron"
    # User said "diğerlerinde Neuron olarak geçecek"
    # We want to match NPP as a standalone word or identifier prefix.
    # But replacing "NPP" globally with "Neuron" is the request.
    # Be careful not to replace inside other words if that's a risk,
    # but NPP is usually a prefix or standalone.
    # We will simply replace NPP with Neuron.
    (r'NPP', r'Neuron'),
]

def should_process_file(filename):
    _, ext = os.path.splitext(filename)
    return ext.lower() not in EXCLUDE_EXTENSIONS

def process_file_content(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        # Binary file skipped
        return False
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False

    original_content = content

    for pattern, replacement in REPLACEMENTS:
        content = re.sub(pattern, replacement, content)

    if content != original_content:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"Updated content: {filepath}")
            return True
        except Exception as e:
            print(f"Error writing {filepath}: {e}")

    return False

def rename_files(root_dir):
    # Walk top-down implies we might rename a directory before entering it?
    # os.walk is top-down by default.
    # Better to rename files first, then directories if needed?
    # Renaming .npp files to .nr

    renamed_count = 0
    for root, dirs, files in os.walk(root_dir):
        # Skip excluded dirs to avoid renaming inside them
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]

        for filename in files:
            if filename.endswith(".npp"):
                old_path = os.path.join(root, filename)
                new_filename = filename[:-4] + ".nr"
                new_path = os.path.join(root, new_filename)

                try:
                    os.rename(old_path, new_path)
                    print(f"Renamed: {old_path} -> {new_path}")
                    renamed_count += 1
                except Exception as e:
                    print(f"Error renaming {old_path}: {e}")

    return renamed_count

def rename_directories(root_dir):
    renamed_count = 0
    # Walk bottom-up to safely rename child dirs then parents
    for root, dirs, files in os.walk(root_dir, topdown=False):
        # DEBUG: Print current root being visited
        # print(f"Processing dirs in: {root}")

        for dirname in dirs:
            if dirname in EXCLUDE_DIRS:
                continue

            new_dirname = dirname

            # Apply branding rules to directory names
            # 1. neuronpp -> neuron
            if "neuronpp" in new_dirname:  # case-sensitive check if on linux, but windows is case-insensitive usually. better cover both or lower()
                new_dirname = new_dirname.replace("neuronpp", "neuron")
            elif "neuronpp" in new_dirname.lower(): # fallback for mixed case?
                 new_dirname = re.sub(r'neuronpp', 'neuron', new_dirname, flags=re.IGNORECASE)

            # 2. NPP -> Neuron (Case sensitive match usually)
            elif "NPP" in new_dirname:
                # Only replace if it stands alone or follows pattern?
                # User said "diğerlerinde Neuron".
                new_dirname = new_dirname.replace("NPP", "Neuron")

            # 3. npp -> neuron
            elif "npp" in new_dirname:
                 new_dirname = new_dirname.replace("npp", "neuron")

            # 4. neuron++ -> neuron (unlikely in dirname but possible)
            elif "neuron++" in new_dirname:
                 new_dirname = new_dirname.replace("neuron++", "neuron")

            if new_dirname != dirname:
                old_path = os.path.join(root, dirname)
                new_path = os.path.join(root, new_dirname)
                try:
                    os.rename(old_path, new_path)
                    print(f"Renamed Directory: {old_path} -> {new_path}")
                    renamed_count += 1
                except Exception as e:
                    print(f"Error renaming directory {old_path}: {e}")

    return renamed_count

def main():
    print("Starting Branding Migration...")
    print(f"Root: {ROOT_DIR}")

    # 1. Update Content
    updated_files = 0
    for root, dirs, files in os.walk(ROOT_DIR):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]

        for file in files:
            if not should_process_file(file):
                continue

            # Skip the script itself
            if file == "migrate_branding.py" or file == "check_branding.py":
                continue

            filepath = os.path.join(root, file)
            if process_file_content(filepath):
                updated_files += 1

    print(f"Content updated in {updated_files} files.")

    # 2. Rename Files
    renamed_files = rename_files(ROOT_DIR)
    print(f"Renamed {renamed_files} files from .npp to .nr")

    # 3. Rename Directories
    renamed_dirs = rename_directories(ROOT_DIR)
    print(f"Renamed {renamed_dirs} directories")

    print("Migration complete.")

if __name__ == "__main__":
    main()

