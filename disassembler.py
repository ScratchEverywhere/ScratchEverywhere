import json
import sys
import os
import re

def parse_opcodes_from_header(header_path):
    try:
        with open(header_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        raise FileNotFoundError(
            f"Opcode-Header Not found: {header_path}\n"
        )

    pattern = r'enum\s+Opcode\s*:\s*uint16_t\s*\{([^}]*)\}'
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        raise ValueError("'enum Opcode' Not found.")

    enum_body = match.group(1)
    tokens = enum_body.split(',')

    opcode_names = []
    for token in tokens:
        token = re.sub(r'//.*$', '', token, flags=re.MULTILINE)
        token = token.strip()
        if not token:
            continue
        if '=' in token:
            name = token.split('=')[0].strip()
        else:
            name = token.strip()
        if name:
            opcode_names.append(name)

    opcode_names = [name for name in opcode_names if name != 'MAX_BLOCKS']

    opcodes = {i: name for i, name in enumerate(opcode_names)}
    return opcodes


OPCODE_HEADER_PATH = os.path.join(
    os.path.dirname(__file__), "source", "runtime", "opcodes", "opcodes.hpp"
)

try:
    OPCODES = parse_opcodes_from_header(OPCODE_HEADER_PATH)
except FileNotFoundError:
    OPCODE_HEADER_PATH = "source/runtime/opcodes/opcodes.hpp"
    OPCODES = parse_opcodes_from_header(OPCODE_HEADER_PATH)


def disassemble(sprite, constants, strings, sprite_infos):
    name = sprite["name"]
    print(f"\n=== Sprite: {name} ===")
    bytecode = sprite["bytecode"]

    var_id_to_name = {v: k for k, v in sprite.get("variables", {}).items()}
    list_id_to_name = {v: k for k, v in sprite.get("lists", {}).items()}

    proc_by_addr = {p["address"]: p["proccode"] for p in sprite.get("procedures", [])}

    sprite_id_to_name = {i: s["name"] for i, s in enumerate(sprite_infos)}

    pc = 0
    while pc < len(bytecode):
        op = bytecode[pc]
        op_name = OPCODES.get(op, f"UNKNOWN_{op}")
        pc += 1
        args = []

        if op_name in ("PUSH_POS_INT", "PUSH_NEG_INT", "PUSH_CONST",
                       "PUSH_PUB_VAR", "PUSH_PRI_VAR",
                       "PUSH_PUB_LIST", "PUSH_PRI_LIST",
                       "PUSH_PROC_ARG",
                       "data_setvariableto_private", "data_setvariableto_public",
                       "data_changevariableby_private", "data_changevariableby_public",
                       "data_lengthoflist_public", "data_lengthoflist_private",
                       "data_deletealloflist_public", "data_deletealloflist_private",
                       "data_deleteoflist_public", "data_deleteoflist_private",
                       "data_insertatlist_public", "data_insertatlist_private",
                       "data_replaceitemoflist_public", "data_replaceitemoflist_private",
                       "data_itemoflist_public", "data_itemoflist_private",
                       "data_itemnumoflist_public", "data_itemnumoflist_private",
                       "data_addtolist_public", "data_addtolist_private",
                       "control_create_clone_of_other"):
            val = bytecode[pc]; pc += 1
            if op_name == "PUSH_POS_INT":
                args.append(str(val))
            elif op_name == "PUSH_NEG_INT":
                args.append(str(-val))
            elif op_name == "PUSH_CONST":
                if val < len(constants):
                    args.append(f"{constants[val]}  # const[{val}]")
                else:
                    args.append(f"<invalid const index {val}>")
            elif op_name in ("PUSH_PUB_VAR", "PUSH_PRI_VAR", "data_setvariableto_private", "data_setvariableto_public", "data_changevariableby_private", "data_changevariableby_public", 
            "data_lengthoflist_public", "data_lengthoflist_private",
            "data_deletealloflist_public", "data_deletealloflist_private",
            "data_deleteoflist_public", "data_deleteoflist_private",
            "data_insertatlist_public", "data_insertatlist_private",
            "data_replaceitemoflist_public", "data_replaceitemoflist_private",
            "data_itemoflist_public", "data_itemoflist_private",
            "data_itemnumoflist_public", "data_itemnumoflist_private",
            "data_addtolist_public", "data_addtolist_private"):
                name = var_id_to_name.get(val, "?")
                args.append(f"id={val} ({name})")
            elif op_name in ("PUSH_PUB_LIST", "PUSH_PRI_LIST"):
                name = list_id_to_name.get(val, "?")
                args.append(f"id={val} ({name})")
            elif op_name == "PUSH_PROC_ARG":
                args.append(f"index={val}")
            elif op_name == "control_create_clone_of_other":
                args.append(f"blueprintId={val}")

        elif op_name in ("motion_goto_sprite", "motion_glideto_sprite"):
            sprite_id = bytecode[pc]; pc += 1
            sprite_name = sprite_id_to_name.get(sprite_id, "?")
            args.append(f"id={sprite_id} ({sprite_name})")

        elif op_name.startswith("JUMP_"):
            offset = bytecode[pc]; pc += 1
            if op_name == "JUMP_BACK":
                target = pc - offset - 2
            else:  # JUMP_FWD, JUMP_FWD_IF_FALSE, JUMP_BACK_IF_FALSE
                target = pc + offset
            args.append(f"offset={offset} -> {target}")

        elif op_name in ("CALL_PROCEDURE", "CALL_PROCEDURE_WOS"):
            arg_count = bytecode[pc]; pc += 1
            lo = bytecode[pc]; hi = bytecode[pc+1]; pc += 2
            addr = lo | (hi << 16)
            proc_name = proc_by_addr.get(addr, "?")
            args.append(f"argc={arg_count} addr={addr} ({proc_name})")

        print(f"{pc-1:04d}: {op_name:30s} {' '.join(args)}")

    if sprite.get("hatListeners"):
        print("\nHatListener:")
        for hl in sprite["hatListeners"]:
            print(f"  offset={hl['offset']} type={hl['type']} eventParamId={hl['eventParamId']}")


def main():
    if len(sys.argv) != 2:
        print("Usage: python disasm.py bytecode_export.json")
        sys.exit(1)
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    constants = data.get("constants", [])
    strings = data.get("strings", [])
    sprites = data.get("sprites", [])

    for sprite in sprites:
        disassemble(sprite, constants, strings, sprites)


if __name__ == "__main__":
    main()