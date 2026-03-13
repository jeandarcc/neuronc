import os
import re
import shutil
import datetime
import tomllib

DIAG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CATALOG_PATH = os.path.join(DIAG_ROOT, "catalog.toml")

def load_catalog():
    with open(CATALOG_PATH, "rb") as f:
        return tomllib.load(f)

def get_required_locales():
    catalog = load_catalog()
    return catalog.get("required_locales", [])

def get_buckets():
    catalog = load_catalog()
    return catalog.get("buckets", {})

def find_bucket_for_code(code):
    buckets = get_buckets()
    for name, config in buckets.items():
        if code in config.get("allowed_exact_codes", []):
            return name
            
    is_warning = code.startswith("W")
    try:
        match = re.search(r'\d+', code)
        if not match: return None
        num = int(match.group())
    except ValueError:
        return None
        
    for name, config in buckets.items():
        range_key = "allowed_w_range" if is_warning else "allowed_n_range"
        if range_key in config:
            r_val = config[range_key]
            if "-" in r_val:
                r_start, r_end = map(int, r_val.split("-"))
                if r_start <= num <= r_end:
                    return name
    return None

def get_registry():
    registry = {}
    locales = get_required_locales()
    buckets = get_buckets()
    for locale in locales:
        for bucket_name, bucket_config in buckets.items():
            path = os.path.join(DIAG_ROOT, bucket_config["path_pattern"].format(locale=locale))
            if os.path.exists(path):
                with open(path, "rb") as f:
                    data = tomllib.load(f)
                    messages = data.get("messages", {})
                    for code, msg in messages.items():
                        if code not in registry:
                            registry[code] = {"bucket": bucket_name, "locales": {}}
                        registry[code]["locales"][locale] = msg
    return registry

def validate_code_format(code):
    return bool(re.match(r'^(N|W)\d+$', code) or re.match(r'^Neuron\d+$', code))

def take_backup():
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    backup_dir = os.path.join(DIAG_ROOT, "plugger", "backups", timestamp)
    os.makedirs(backup_dir, exist_ok=True)
    locales = get_required_locales()
    buckets = get_buckets()
    for locale in locales:
        locale_dir = os.path.join(backup_dir, locale)
        os.makedirs(locale_dir, exist_ok=True)
        for bucket_config in buckets.values():
            src = os.path.join(DIAG_ROOT, bucket_config["path_pattern"].format(locale=locale))
            if os.path.exists(src):
                shutil.copy2(src, os.path.join(locale_dir, os.path.basename(src)))
    return backup_dir

def apply_changes(plugger_data):
    meta = plugger_data["meta"]
    code = meta["code"]
    bucket_name = meta["bucket"]
    action = meta["action"]
    parameters = meta.get("parameters", [])
    buckets = get_buckets()
    bucket_config = buckets[bucket_name]
    locales = get_required_locales()
    take_backup()
    for locale in locales:
        file_path = os.path.join(DIAG_ROOT, bucket_config["path_pattern"].format(locale=locale))
        locale_msg = plugger_data["locale"][locale]
        tags_val = str(meta["tags"]).replace("'", '"')
        msg_block = [
            f"\n[messages.{code}]",
            f'severity = "{meta["severity"]}"',
            f'phase = "{meta["phase"]}"',
            f'slug = "{meta["slug"]}"',
            f'title = "{locale_msg["title"]}"',
            f'summary = "{locale_msg["summary"]}"',
            f'default_message = "{locale_msg["default_message"]}"',
        ]
        if parameters:
            msg_block.append(
                "parameters = " + str(parameters).replace("'", '"'))
        msg_block.extend([
            f'recovery = "{locale_msg["recovery"]}"',
            f'tags = {tags_val}'
        ])
        new_content = "\n".join(msg_block) + "\n"
        with open(file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
        if action == "add":
            with open(file_path, "a", encoding="utf-8") as f:
                f.write(new_content)
        else:
            start_line = -1
            end_line = -1
            pattern = f"[messages.{code}]"
            for i, line in enumerate(lines):
                if line.strip() == pattern:
                    start_line = i
                    for j in range(i + 1, len(lines)):
                        if lines[j].strip().startswith("["):
                            end_line = j
                            break
                    if end_line == -1: end_line = len(lines)
                    break
            if start_line != -1:
                lines[start_line:end_line] = [new_content]
                with open(file_path, "w", encoding="utf-8") as f:
                    f.writelines(lines)
            else:
                raise ValueError(f"Code {code} not found in {file_path}")

def validate_plugger_toml(data):
    required_meta = ["action", "code", "bucket", "severity", "phase", "slug", "tags"]
    for key in required_meta:
        if key not in data.get("meta", {}):
            return False, f"Missing meta key: {key}"
    required_locales = get_required_locales()
    locale_data = data.get("locale", {})
    if not locale_data: return False, "Missing 'locale' section"
    for loc in required_locales:
        if loc not in locale_data:
            return False, f"Missing locale: {loc}"
        fields = ["title", "summary", "default_message", "recovery"]
        for field in fields:
            if field not in locale_data[loc] or not str(locale_data[loc][field]).strip():
                return False, f"Missing or empty field '{field}' in locale '{loc}'"
    if not re.match(r'^[a-z0-9_]+$', data["meta"]["slug"]):
        return False, "Slug must be snake_case (lowercase, digits, underscores)"
    if data["meta"]["severity"] not in ["error", "warning"]:
        return False, "Severity must be 'error' or 'warning'"
    parameters = data["meta"].get("parameters", [])
    if not isinstance(parameters, list) or any(
            not isinstance(item, str) or not item.strip() for item in parameters):
        return False, "Parameters must be an array of non-empty strings"
    return True, ""
