import os
import sys
import shutil
import tomllib
import plugger_ops

PENDING_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pending")

def show_help():
    print("Usage:")
    print("  python plugger.py add")
    print("  python plugger.py update <CODE>")
    print("  python plugger.py apply <PATH_TO_PENDING_TOML>")

def cmd_add():
    code = input("Enter new diagnostic code (e.g. N1099, W2003): ").strip().upper()
    if not plugger_ops.validate_code_format(code):
        print(f"Error: Invalid code format: {code}")
        return
    registry = plugger_ops.get_registry()
    if code in registry:
        print(f"Error: Code {code} already exists in bucket {registry[code]['bucket']}")
        return
    bucket = plugger_ops.find_bucket_for_code(code)
    if not bucket:
        print(f"Error: Code {code} does not fall into any bucket's allowed range in catalog.toml")
        return
    template_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "plugger_template.toml")
    dest_path = os.path.join(PENDING_DIR, f"ADD_{code}.toml")
    if not os.path.exists(PENDING_DIR): os.makedirs(PENDING_DIR)
    with open(template_path, "r", encoding="utf-8") as f: content = f.read()
    content = content.replace('code     = "NXXXX"', f'code     = "{code}"')
    content = content.replace('bucket   = "BUCKET"', f'bucket   = "{bucket}"')
    with open(dest_path, "w", encoding="utf-8") as f: f.write(content)
    print(f"Template created: {dest_path}")
    print("Please edit the file and then run:")
    print(f"  python plugger.py apply pending/ADD_{code}.toml")

def cmd_update(code):
    code = code.strip().upper()
    registry = plugger_ops.get_registry()
    if code not in registry:
        print(f"Error: Code {code} not found in registry."); return
    bucket = registry[code]["bucket"]
    template_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "plugger_template.toml")
    dest_path = os.path.join(PENDING_DIR, f"UPDATE_{code}.toml")
    if not os.path.exists(PENDING_DIR): os.makedirs(PENDING_DIR)
    with open(template_path, "r", encoding="utf-8") as f: lines = f.readlines()
    new_lines = []
    meta_done = False
    for line in lines:
        if not meta_done:
            if 'action   = "add"' in line: line = line.replace('"add"', '"update"')
            if 'code     = "NXXXX"' in line: line = f'code     = "{code}"\n'
            if 'bucket   = "BUCKET"' in line: line = f'bucket   = "{bucket}"\n'
            first_loc = list(registry[code]["locales"].values())[0]
            if 'severity = "error"' in line: line = f'severity = "{first_loc["severity"]}"\n'
            if 'phase    = "parser"' in line: line = f'phase    = "{first_loc["phase"]}"\n'
            if 'slug     = "my_slug"' in line: line = f'slug     = "{first_loc["slug"]}"\n'
            if 'tags     = ["tag1", "tag2"]' in line:
                tags_str = str(first_loc["tags"]).replace("'", '"')
                line = f'tags     = {tags_str}\n'
            if 'parameters = []' in line:
                parameters_str = str(first_loc.get("parameters", [])).replace("'", '"')
                line = f'parameters = {parameters_str}\n'
            if line.startswith("[locale."): meta_done = True
        if meta_done and line.startswith("[locale."):
            loc = line.split(".")[1].split("]")[0]
            msg_data = registry[code]["locales"].get(loc, {})
            new_lines.append(f"[locale.{loc}]\n")
            new_lines.append(f'title           = "{msg_data.get("title", "")}"\n')
            new_lines.append(f'summary         = "{msg_data.get("summary", "")}"\n')
            new_lines.append(f'default_message = "{msg_data.get("default_message", "")}"\n')
            new_lines.append(f'recovery        = "{msg_data.get("recovery", "")}"\n\n')
            continue
        if not meta_done or (not line.startswith("title") and not line.startswith("summary") and not line.startswith("default_message") and not line.startswith("recovery")):
            new_lines.append(line)
    with open(dest_path, "w", encoding="utf-8") as f: f.writelines(new_lines)
    print(f"Update template created: {dest_path}")
    print("Please edit the file and then run:")
    print(f"  python plugger.py apply pending/UPDATE_{code}.toml")

def cmd_apply(path):
    if not os.path.exists(path):
        print(f"Error: File not found: {path}"); return
    with open(path, "rb") as f:
        try: data = tomllib.load(f)
        except Exception as e:
            print(f"Error parsing TOML: {e}"); return
    ok, err = plugger_ops.validate_plugger_toml(data)
    if not ok:
        print(f"Validation Error: {err}"); return
    try:
        plugger_ops.apply_changes(data)
        print("Successfully applied changes to all locale files.")
        shutil.move(path, path + ".done")
    except Exception as e:
        print(f"Error applying changes: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        show_help(); sys.exit(0)
    cmd = sys.argv[1].lower()
    if cmd == "add": cmd_add()
    elif cmd == "update" and len(sys.argv) == 3: cmd_update(sys.argv[2])
    elif cmd == "apply" and len(sys.argv) == 3: cmd_apply(sys.argv[2])
    else: show_help()
