import os

def strip_bom_from_file(file_path):
    try:
        with open(file_path, 'rb') as f:
            raw = f.read()
        
        # Check for UTF-8 BOM
        if raw.startswith(b'\xef\xbb\xbf'):
            print(f"Stripping BOM from: {file_path}")
            with open(file_path, 'wb') as f:
                f.write(raw[3:])
            return True
    except Exception as e:
        print(f"Error {file_path}: {e}")
    return False

def run():
    # Targets known problematic areas
    targets = [
        'runtime/minimal',
        'config/diagnostics',
        'tests'
    ]
    
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    count = 0
    
    for t in targets:
        target_path = os.path.join(project_root, t)
        if not os.path.exists(target_path): continue
        
        for root, _, files in os.walk(target_path):
            for file in files:
                if file.endswith(('.toml', '.manifest', '.cpp', '.h', '.nr')):
                    if strip_bom_from_file(os.path.join(root, file)):
                        count += 1
    
    print(f"\nBOM Cleanup finished. {count} files fixed.")

if __name__ == "__main__":
    run()
