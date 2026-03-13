import os
import re

def normalize_toml_file(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Regex to find [messages.nrXXXX] and convert to [messages.NRXXXX]
        # and also fix numeric ones like [messages.n2001]
        pattern = r'\[messages\.(n[r\d][\da-fA-F]+)\]'
        
        def replacer(match):
            original = match.group(1)
            # Convert to uppercase (NRXXXX or NXXXX)
            fixed = original.upper()
            return f'[messages.{fixed}]'
        
        new_content = re.sub(pattern, replacer, content)
        
        if new_content != content:
            print(f"Normalized keys in: {file_path}")
            with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(new_content)
            return True
    except Exception as e:
        print(f"Error {file_path}: {e}")
    return False

def run():
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    diag_dir = os.path.join(project_root, "config", "diagnostics")
    
    count = 0
    for root, _, files in os.walk(diag_dir):
        for file in files:
            if file.endswith('.toml'):
                if normalize_toml_file(os.path.join(root, file)):
                    count += 1
    
    print(f"\nDiagnostic normalization finished. {count} files fixed.")

if __name__ == "__main__":
    run()
