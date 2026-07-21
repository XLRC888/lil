#include "lil.h"

#define MAX_MC_PROPS 128
#define MAX_ITEMS 128
#define MAX_BLOCKS 64
#define MAX_RECIPES 64
#define MAX_TABS 8
#define MAX_MATERIALS 8
#define MAX_TIERS 8

typedef struct {
    char name[128];
    char *value;
} McProp;

typedef struct {
    char *name;
    char *type;
    char *filename;
    char *material_name;
    ASTNode *prop_block;
    ASTNode *proc_block;
    char *proc_event;
    int item_index;
    char *display_name;
    int is_food;
    int has_effects;
} McItem;

typedef struct {
    char *name;
    char *filename;
    ASTNode *prop_block;
    ASTNode *proc_block;
    char *proc_event;
    int item_index;
    char *display_name;
} McBlock;

typedef struct {
    char *name;
    int type;
    char *result;
    int count;
    char *pattern_rows[4];
    int nrows;
    char keys[16];
    char *key_values[16];
    int nkeys;
    char *ingredients[16];
    int ningredients;
    char *ingredient;
    float experience;
    int cook_time;
    char *group;
    int is_blast;
} McRecipe;

typedef struct {
    char *name;
    char *icon;
    char *items[64];
    int nitems;
} McCreativeTab;

typedef struct {
    char *name;
    int durability;
    int protection[4];
    float toughness;
    float knockback;
    char *repair_item;
    char *equip_sound;
} McArmorMaterial;

typedef struct {
    char *name;
    int level;
    int uses;
    float speed;
    float damage;
    int enchantability;
    char *repair_item;
} McToolTier;

#define MAX_MODEL_TEX 8

typedef struct {
    char parent[64];
    char tex_keys[MAX_MODEL_TEX][32];
    char tex_vals[MAX_MODEL_TEX][64];
    int ntex;
} McModel;

static McProp mc_props[MAX_MC_PROPS];
static int mc_prop_count;
static McItem mc_items[MAX_ITEMS];
static int mc_item_count;
static McBlock mc_blocks[MAX_BLOCKS];
static int mc_block_count;
static McRecipe mc_recipes[MAX_RECIPES];
static int mc_recipe_count;
static McCreativeTab mc_tabs[MAX_TABS];
static int mc_tab_count;
static McArmorMaterial mc_armor_materials[MAX_MATERIALS];
static int mc_armor_material_count;
static McToolTier mc_tool_tiers[MAX_TIERS];
static int mc_tool_tier_count;
static char mc_mod_id[64];
static char mc_mod_pkg[256];
static char mc_output_filename[256];
static int gradle_properties_mode;
static int mc_name_seq_counter;
static int mc_name_consumer;
static char *mc_item_display_names[MAX_ITEMS];
static char *mc_tab_display_names[MAX_TABS];
static McModel mc_item_models[MAX_ITEMS];
static McModel mc_block_models[MAX_BLOCKS];

static void mc_add_prop(const char *name, const char *value) {
    if (mc_prop_count >= MAX_MC_PROPS) return;
    snprintf(mc_props[mc_prop_count].name, sizeof(mc_props[mc_prop_count].name), "%s", name);
    mc_props[mc_prop_count].value = sdup(value);
    mc_prop_count++;
}

static void mc_add_recipe_shaped(const char *name, const char *result, int count,
    const char *rows[4], int nrows, const char *keys, const char *keyvals[16], int nkeys,
    const char *group) {
    if (mc_recipe_count >= MAX_RECIPES) return;
    McRecipe *r = &mc_recipes[mc_recipe_count++];
    memset(r, 0, sizeof(McRecipe));
    r->name = sdup(name);
    r->type = 0;
    if (result) r->result = sdup(result);
    r->count = count > 0 ? count : 1;
    r->nrows = nrows;
    for (int i = 0; i < nrows && i < 4; i++)
        r->pattern_rows[i] = sdup(rows[i]);
    r->nkeys = nkeys;
    for (int i = 0; i < nkeys && i < 16; i++) {
        r->keys[i] = keys[i];
        if (keyvals[i]) r->key_values[i] = sdup(keyvals[i]);
    }
    if (group) r->group = sdup(group);
}

static void mc_add_recipe_shapeless(const char *name, const char *result, int count,
    const char *ingredients[16], int ning, const char *group) {
    if (mc_recipe_count >= MAX_RECIPES) return;
    McRecipe *r = &mc_recipes[mc_recipe_count++];
    memset(r, 0, sizeof(McRecipe));
    r->name = sdup(name);
    r->type = 1;
    if (result) r->result = sdup(result);
    r->count = count > 0 ? count : 1;
    r->ningredients = ning;
    for (int i = 0; i < ning && i < 16; i++)
        if (ingredients[i]) r->ingredients[i] = sdup(ingredients[i]);
    if (group) r->group = sdup(group);
}

static void mc_add_tab(McCreativeTab *t) {
    if (mc_tab_count < MAX_TABS) {
        memcpy(&mc_tabs[mc_tab_count], t, sizeof(McCreativeTab));
        mc_tab_count++;
    }
}

static void mc_add_recipe_smelting(const char *name, const char *ingredient, const char *result,
    float exp, int cook, const char *group) {
    if (mc_recipe_count >= MAX_RECIPES) return;
    McRecipe *r = &mc_recipes[mc_recipe_count++];
    memset(r, 0, sizeof(McRecipe));
    r->name = sdup(name);
    r->type = 2;
    if (ingredient) r->ingredient = sdup(ingredient);
    if (result) r->result = sdup(result);
    r->experience = exp;
    r->cook_time = cook > 0 ? cook : 200;
    if (group) r->group = sdup(group);
}

static int mc_find_material(const char *name) {
    for (int i = 0; i < mc_armor_material_count; i++)
        if (!strcmp(mc_armor_materials[i].name, name)) return i;
    return -1;
}

static int mc_find_tier(const char *name) {
    for (int i = 0; i < mc_tool_tier_count; i++)
        if (!strcmp(mc_tool_tiers[i].name, name)) return i;
    return -1;
}

static int mc_ensure_material(const char *name, int dur, int p0, int p1, int p2, int p3, float tough, float kb) {
    int idx = mc_find_material(name);
    if (idx >= 0) return idx;
    if (mc_armor_material_count >= MAX_MATERIALS) return -1;
    int i = mc_armor_material_count++;
    mc_armor_materials[i].name = sdup(name);
    mc_armor_materials[i].durability = dur > 0 ? dur : 15;
    mc_armor_materials[i].protection[0] = p0;
    mc_armor_materials[i].protection[1] = p1;
    mc_armor_materials[i].protection[2] = p2;
    mc_armor_materials[i].protection[3] = p3;
    mc_armor_materials[i].toughness = tough;
    mc_armor_materials[i].knockback = kb;
    mc_armor_materials[i].repair_item = sdup(name);
    mc_armor_materials[i].equip_sound = sdup("diamond");
    return i;
}

static int mc_ensure_tier(const char *name, int level, int uses, float speed, float damage, int enchant) {
    int idx = mc_find_tier(name);
    if (idx >= 0) return idx;
    if (mc_tool_tier_count >= MAX_TIERS) return -1;
    int i = mc_tool_tier_count++;
    mc_tool_tiers[i].name = sdup(name);
    mc_tool_tiers[i].level = level;
    mc_tool_tiers[i].uses = uses > 0 ? uses : 500;
    mc_tool_tiers[i].speed = speed;
    mc_tool_tiers[i].damage = damage;
    mc_tool_tiers[i].enchantability = enchant > 0 ? enchant : 10;
    mc_tool_tiers[i].repair_item = sdup(name);
    return i;
}

static void scan_mc_all(const char *src) {
    const char *p = src;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        const char *start = p;

        if (*p == 'f' && strncmp(p, "foodEffect@minecraft", 20) == 0) {
            p += 20;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '(') {
                p++;
                char vals[8][64]; int nvals = 0;
                memset(vals, 0, sizeof(vals));
                while (*p && *p != ')' && nvals < 8) {
                    while (*p == ' ' || *p == '\t' || *p == ',') p++;
                    if (*p == ')') break;
                    if (*p == '"') {
                        p++; int vi = 0;
                        while (*p && *p != '"' && vi < 60) vals[nvals][vi++] = *p++;
                        if (*p == '"') p++;
                        vals[nvals][vi] = 0; nvals++;
                    } else if (*p >= '0' && *p <= '9') {
                        int vi = 0;
                        while (*p && *p != ',' && *p != ')' && *p != ' ' && vi < 60) vals[nvals][vi++] = *p++;
                        vals[nvals][vi] = 0; nvals++;
                    } else {
                        int vi = 0;
                        while (*p && *p != ',' && *p != ')' && *p != ' ' && vi < 60) vals[nvals][vi++] = *p++;
                        vals[nvals][vi] = 0; nvals++;
                    }
                }
                if (nvals >= 4) {
                }
            }
            goto scan_line;
        }

        if (strncmp(p, "recipeShaped@minecraft", 22) == 0) { p += 22;
            goto scan_recipe_block;
        }
        if (strncmp(p, "recipeShapeless@minecraft", 25) == 0) { p += 25;
            goto scan_recipe_block;
        }
        if (strncmp(p, "recipeSmelting@minecraft", 24) == 0) { p += 24;
            goto scan_recipe_block;
        }
        if (strncmp(p, "newCreativeTab@minecraft", 24) == 0) { p += 24;
            goto scan_tab_block;
        }

        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '=' && *p != '@' && *p != '{') p++;
        if (*p == '@' && strncmp(p, "@minecraft", 10) == 0) {
            const char *name_start = start;
            size_t name_len = p - start;
            p += 10;
            while (*p == ' ' || *p == '\t') p++;

            if (*p == '{') {
                p++;
                if (name_len == 9 && strncmp(name_start, "itemNames", 9) == 0) {
                    int depth = 1;
                    while (*p && depth > 0) {
                        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                        if (*p == '}') { depth--; if (!depth) break; p++; continue; }
                        if (*p == '[') {
                            p++; int idx = 0;
                            while (*p >= '0' && *p <= '9') idx = idx * 10 + (*p++ - '0');
                            if (*p == ']') p++;
                            while (*p == ' ' || *p == '\t') p++;
                            if (*p == '=') p++;
                            while (*p == ' ' || *p == '\t') p++;
                            char val[256]; int vi = 0;
                            if (*p == '"') {
                                p++;
                                while (*p && *p != '"' && vi < 250) val[vi++] = *p++;
                                if (*p == '"') p++;
                            } else {
                                while (*p && *p != '\n' && *p != '\r' && vi < 250) val[vi++] = *p++;
                            }
                            val[vi] = 0;
                                if (idx > 0 && idx <= MAX_ITEMS) {
                                if (mc_name_seq_counter < MAX_ITEMS) {
                                    if (mc_item_display_names[mc_name_seq_counter]) free(mc_item_display_names[mc_name_seq_counter]);
                                    mc_item_display_names[mc_name_seq_counter] = sdup(val);
                                    mc_name_seq_counter++;
                                }
                            }
                        }
                        while (*p && *p != '\n' && *p != '\r' && *p != '}') { if (*p == '{') depth++; p++; }
                    }
                    if (*p == '}') p++;
                } else if (name_len == 11 && strncmp(name_start, "modelsBlock", 11) == 0) {
                    int depth = 1;
                    while (*p && depth > 0) {
                        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                        if (*p == '}') { depth--; if (!depth) break; p++; continue; }
                        if (*p == '[') {
                            p++; int idx = 0;
                            while (*p >= '0' && *p <= '9') idx = idx * 10 + (*p++ - '0');
                            if (*p == ']') p++;
                            while (*p == ' ' || *p == '\t') p++;
                            char key[32]; int ki = 0;
                            while (*p && *p != ' ' && *p != '\t' && *p != '=' && ki < 30) key[ki++] = *p++;
                            key[ki] = 0;
                            while (*p == ' ' || *p == '\t' || *p == '=') p++;
                            while (*p == ' ' || *p == '\t') p++;
                            char val[64]; int vi = 0;
                            if (*p == '"') { p++; while (*p && *p != '"' && vi < 60) val[vi++] = *p++; if (*p == '"') p++; }
                            else { while (*p && *p != '\n' && *p != '\r' && *p != ' ' && vi < 60) val[vi++] = *p++; }
                            val[vi] = 0;
                            if (idx > 0 && idx <= MAX_BLOCKS) {
                                int mi = idx - 1;
                                if (!strcmp(key, "parent"))
                                    snprintf(mc_block_models[mi].parent, sizeof(mc_block_models[mi].parent), "%s", val);
                                else if (mc_block_models[mi].ntex < MAX_MODEL_TEX) {
                                    int ti = mc_block_models[mi].ntex++;
                                    snprintf(mc_block_models[mi].tex_keys[ti], sizeof(mc_block_models[mi].tex_keys[ti]), "%s", key);
                                    snprintf(mc_block_models[mi].tex_vals[ti], sizeof(mc_block_models[mi].tex_vals[ti]), "%s", val);
                                }
                            }
                        }
                        while (*p && *p != '\n' && *p != '\r') p++;
                    }
                    if (*p == '}') p++;
                } else if (name_len == 10 && strncmp(name_start, "modelsItem", 10) == 0) {
                    int depth = 1;
                    while (*p && depth > 0) {
                        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                        if (*p == '}') { depth--; if (!depth) break; p++; continue; }
                        if (*p == '[') {
                            p++; int idx = 0;
                            while (*p >= '0' && *p <= '9') idx = idx * 10 + (*p++ - '0');
                            if (*p == ']') p++;
                            while (*p == ' ' || *p == '\t') p++;
                            char key[32]; int ki = 0;
                            while (*p && *p != ' ' && *p != '\t' && *p != '=' && ki < 30) key[ki++] = *p++;
                            key[ki] = 0;
                            while (*p == ' ' || *p == '\t' || *p == '=') p++;
                            while (*p == ' ' || *p == '\t') p++;
                            char val[64]; int vi = 0;
                            if (*p == '"') { p++; while (*p && *p != '"' && vi < 60) val[vi++] = *p++; if (*p == '"') p++; }
                            else { while (*p && *p != '\n' && *p != '\r' && *p != ' ' && vi < 60) val[vi++] = *p++; }
                            val[vi] = 0;
                            if (idx > 0 && idx <= MAX_ITEMS) {
                                int mi = idx - 1;
                                if (!strcmp(key, "parent"))
                                    snprintf(mc_item_models[mi].parent, sizeof(mc_item_models[mi].parent), "%s", val);
                                else if (mc_item_models[mi].ntex < MAX_MODEL_TEX) {
                                    int ti = mc_item_models[mi].ntex++;
                                    snprintf(mc_item_models[mi].tex_keys[ti], sizeof(mc_item_models[mi].tex_keys[ti]), "%s", key);
                                    snprintf(mc_item_models[mi].tex_vals[ti], sizeof(mc_item_models[mi].tex_vals[ti]), "%s", val);
                                }
                            }
                        }
                        while (*p && *p != '\n' && *p != '\r') p++;
                    }
                    if (*p == '}') p++;
                } else {
                    int depth = 1;
                    while (*p && depth > 0) {
                        if (*p == '{') depth++;
                        if (*p == '}') { depth--; if (!depth) break; }
                        p++;
                    }
                    if (*p == '}') p++;
                }
                while (*p && *p != '\n' && *p != '\r') p++;
                continue;
            }

            if (*p == '=') {
                p++;
                while (*p == ' ' || *p == '\t') p++;
                char *merged = malloc(4096);
                merged[0] = 0;
                size_t mpos = 0;
                while (*p && *p != '\n' && *p != '\r') {
                    while (*p == ' ' || *p == '\t') p++;
                    if (!*p || *p == '\n' || *p == '\r') break;
                    if (mpos > 0) { merged[mpos++] = ' '; merged[mpos] = 0; }
                    if (*p == '"') {
                        p++;
                        while (*p && *p != '"' && mpos < 4090) merged[mpos++] = *p++;
                        if (*p == '"') p++;
                    } else {
                        while (*p && *p != ',' && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t' && mpos < 4090)
                            merged[mpos++] = *p++;
                    }
                    merged[mpos] = 0;
                    if (*p == ',') { p++; while (*p == ' ' || *p == '\t') p++; }
                    else break;
                }
                char name_buf[128];
                snprintf(name_buf, sizeof(name_buf), "%.*s", (int)name_len, name_start);
                mc_add_prop(name_buf, merged);
                free(merged);
            }
        } else if (p - start >= 7 && strncmp(p - 7, "pattern", 7) == 0
            && (p - start == 7 || *(p-8) == ' ' || *(p-8) == '\t' || *(p-8) == '\n')) {
            p++;
            char cells[16][32]; int ncells = 0;
            memset(cells, 0, sizeof(cells));
            while (*p && ncells < 16) {
                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
                if (*p == '}') break;
                if (*p == '"') {
                    p++; int ri = 0;
                    while (*p && *p != '"' && ri < 30) cells[ncells][ri++] = *p++;
                    if (*p == '"') p++;
                    cells[ncells][ri] = 0; ncells++;
                } else {
                    while (*p && *p != '}' && *p != '\n') p++;
                }
            }
            if (ncells > 0) {
                char rows[4][32]; int nrows = 0;
                int per = (ncells == 9) ? 3 : (ncells == 4) ? 2 : 1;
                for (int r = 0; r < (per > 1 ? ncells / per : ncells) && r < 4; r++) {
                    int pos = 0;
                    if (per > 1) {
                        for (int c = 0; c < per; c++) {
                            int idx = r * per + c;
                            rows[r][pos++] = cells[idx][0] ? cells[idx][0] : ' ';
                        }
                    } else {
                        int si = 0; while (cells[r][si] && pos < 31) rows[r][pos++] = cells[r][si++];
                    }
                    rows[r][pos] = 0; nrows++;
                }
                if (nrows > 0) {
                    const char *rrows[4];
                    for (int i = 0; i < nrows; i++) rrows[i] = rows[i];
                    mc_add_recipe_shaped("_pattern", NULL, 0, rrows, nrows, NULL, NULL, 0, NULL);
                }
            }
            if (*p == '}') p++;
        }
        goto scan_line;

        scan_recipe_block:
        {
            while (*p == ' ' || *p == '\t') p++;
            char rname[64]; int rni = 0;
            if (*p == '(') {
                p++;
                while (*p && *p != ')' && *p != '(' && rni < 60) rname[rni++] = *p++;
                if (*p == ')') p++;
            }
            rname[rni] = 0;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '{') {
                p++;
                char result[64] = "", ingredient[64] = "", group[64] = "";
                char keys_chars[16], *key_vals[16];
                char ingredients[16][64];
                char pattern_rows[4][32]; int nrows = 0;
                int ning = 0, nk = 0, count = 0, cook = 0;
                float exp = 0;
                int is_smelting = (start[6] == 'S' && start[7] == 'm');
                int is_shapeless = (start[6] == 'S' && start[7] == 'h' && start[11] == 'l');

                memset(keys_chars, 0, sizeof(keys_chars));
                memset(key_vals, 0, sizeof(key_vals));
                memset(ingredients, 0, sizeof(ingredients));

                int depth = 1;
                while (*p && depth > 0) {
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                    if (*p == '}') { depth--; if (!depth) break; continue; }
                    if (*p == '{') { depth++; p++; continue; }

                    if (strncmp(p, "pattern", 7) == 0) {
                        p += 7;
                        while (*p != '{' && *p != '\n' && *p != '}') p++;
                        if (*p == '{') {
                            p++;
                            char rcells[16][32]; int rnc = 0;
                            memset(rcells, 0, sizeof(rcells));
                            while (*p && rnc < 16) {
                                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
                                if (*p == '}') break;
                                if (*p == '"') {
                                    p++; int ri = 0;
                                    while (*p && *p != '"' && ri < 30) rcells[rnc][ri++] = *p++;
                                    if (*p == '"') p++;
                                    rcells[rnc][ri] = 0; rnc++;
                                }
                            }
                            nrows = 0;
                            memset(pattern_rows, 0, sizeof(pattern_rows));
                            int per = (rnc == 9) ? 3 : (rnc == 4) ? 2 : 1;
                            for (int r = 0; r < (per > 1 ? rnc / per : rnc) && r < 4; r++) {
                                int pos = 0;
                                if (per > 1) {
                                    for (int c = 0; c < per; c++) {
                                        int idx = r * per + c;
                                        pattern_rows[nrows][pos++] = rcells[idx][0] ? rcells[idx][0] : ' ';
                                    }
                                } else {
                                    int si = 0; while (rcells[r][si] && pos < 31) pattern_rows[nrows][pos++] = rcells[r][si++];
                                }
                                pattern_rows[nrows][pos] = 0; nrows++;
                            }
                            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                            if (*p == '}') p++;
                        }
                        continue;
                    }

                    if (strncmp(p, "result", 6) == 0 && (p[6] == ' ' || p[6] == '=')) {
                        p += 6;
                        while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        int ri = 0;
                        if (*p == '"') { p++; while (*p && *p != '"' && ri < 60) result[ri++] = *p++; if (*p == '"') p++; }
                        else { while (*p && *p != '\n' && *p != '\r' && *p != ' ' && ri < 60) result[ri++] = *p++; }
                        result[ri] = 0; continue;
                    }
                    if (strncmp(p, "count", 5) == 0 && (p[5] == ' ' || p[5] == '=')) {
                        p += 5; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        count = 0; while (*p >= '0' && *p <= '9') count = count*10 + (*p++ - '0'); continue;
                    }
                    if (strncmp(p, "experience", 10) == 0 && (p[10] == ' ' || p[10] == '=')) {
                        p += 10; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        exp = (float)strtod(p, (char**)&p); continue;
                    }
                    if (strncmp(p, "cookTime", 8) == 0 && (p[8] == ' ' || p[8] == '=')) {
                        p += 8; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        cook = 0; while (*p >= '0' && *p <= '9') cook = cook*10 + (*p++ - '0'); continue;
                    }
                    if (strncmp(p, "group", 5) == 0 && (p[5] == ' ' || p[5] == '=')) {
                        p += 5; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        int gi = 0;
                        if (*p == '"') { p++; while (*p && *p != '"' && gi < 60) group[gi++] = *p++; if (*p == '"') p++; }
                        else { while (*p && *p != '\n' && *p != '\r' && gi < 60) group[gi++] = *p++; }
                        group[gi] = 0; continue;
                    }
                    if (strncmp(p, "ingredient", 10) == 0 && (p[10] == ' ' || p[10] == '=')) {
                        p += 10; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        int gi = 0;
                        if (*p == '"') { p++; while (*p && *p != '"' && gi < 60) ingredient[gi++] = *p++; if (*p == '"') p++; }
                        else { while (*p && *p != '\n' && *p != '\r' && *p != ' ' && gi < 60) ingredient[gi++] = *p++; }
                        ingredient[gi] = 0; continue;
                    }
                    if (strncmp(p, "ingredients", 11) == 0 && (p[11] == ' ' || p[11] == '=')) {
                        p += 11; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        while (*p && *p != '\n' && *p != '\r') {
                            while (*p == ' ' || *p == '\t' || *p == ',') p++;
                            if (*p == '\n' || *p == '\r') break;
                            if (*p == '"') {
                                p++; int gi = 0;
                                while (*p && *p != '"' && gi < 60) ingredients[ning][gi++] = *p++;
                                if (*p == '"') p++;
                                ingredients[ning][gi] = 0;
                                if (ning < 16) ning++;
                            } else {
                                int gi = 0;
                                while (*p && *p != ',' && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t' && gi < 60)
                                    ingredients[ning][gi++] = *p++;
                                ingredients[ning][gi] = 0;
                                if (ning < 16) ning++;
                            }
                        } continue;
                    }
                    if (strncmp(p, "keys", 4) == 0 && (p[4] == ' ' || p[4] == '=')) {
                        p += 4; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                        while (*p && *p != '\n' && *p != '\r') {
                            while (*p == ' ' || *p == '\t' || *p == ',') p++;
                            if (*p == '\n' || *p == '\r') break;
                            if (*p == '"') {
                                p++;
                                if (nk < 16) keys_chars[nk] = *p;
                                p += 2;
                                while (*p == ' ' || *p == '\t' || *p == ':') p++;
                                if (*p == ':') p++;
                                while (*p == ' ' || *p == '\t') p++;
                                if (*p == '"') {
                                    p++; char kv[64]; int ki = 0;
                                    while (*p && *p != '"' && ki < 60) kv[ki++] = *p++;
                                    if (*p == '"') p++;
                                    kv[ki] = 0;
                                    if (nk < 16) { key_vals[nk] = sdup(kv); nk++; }
                                }
                            }
                        } continue;
                    }
                    while (*p && *p != '\n' && *p != '\r') p++;
                }

                if (is_smelting) {
                    mc_add_recipe_smelting(rname, ingredient, result, exp, cook, group[0] ? group : NULL);
                } else if (is_shapeless) {
                    const char *ing[16];
                    for (int i = 0; i < ning; i++) ing[i] = ingredients[i];
                    mc_add_recipe_shapeless(rname, result, count, ing, ning, group[0] ? group : NULL);
                } else {
                    const char *r_rows[4] = {NULL};
                    for (int i = 0; i < nrows && i < 4; i++) r_rows[i] = pattern_rows[i];
                    mc_add_recipe_shaped(rname, result, count, r_rows, nrows,
                        keys_chars, (const char**)key_vals, nk, group[0] ? group : NULL);
                }
                if (*p == '}') p++;
            }
        }
        goto end_line;

        scan_tab_block:
        {
            while (*p == ' ' || *p == '\t') p++;
            char tnames[MAX_TABS][64];
            int ntabs = 0;
            memset(tnames, 0, sizeof(tnames));
            if (*p == '(') {
                p++;
                while (*p && *p != ')' && ntabs < MAX_TABS) {
                    while (*p == ' ' || *p == '\t' || *p == ',') p++;
                    if (*p == ')' || !*p) break;
                    int j = 0;
                    while (*p && *p != ')' && *p != ',' && *p != ' ' && *p != '\t' && j < 60)
                        tnames[ntabs][j++] = *p++;
                    if (j > 0) ntabs++;
                }
                if (*p == ')') p++;
            }
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '{') {
                int depth = 1; p++;
                char icon[64] = ""; int icon_set = 0;
                char items_list[64][64]; int nitems = 0;
                char tab_dnames[MAX_TABS][128];
                memset(tab_dnames, 0, sizeof(tab_dnames));
                memset(items_list, 0, sizeof(items_list));
                while (*p && depth > 0) {
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                    if (*p == '}') { depth--; if (!depth) break; continue; }

                    if (strncmp(p, "creativeTabNames@minecraft", 26) == 0) {
                        p += 26;
                        while (*p == ' ' || *p == '\t') p++;
                        if (*p == '{') { p++;
                            int sd = 1;
                            while (*p && sd > 0) {
                                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                                if (*p == '}') { sd--; if (!sd) break; p++; continue; }
                                if (*p == '[') {
                                    p++; int idx = 0;
                                    while (*p >= '0' && *p <= '9') idx = idx * 10 + (*p++ - '0');
                                    if (*p == ']') p++;
                                    while (*p == ' ' || *p == '\t') p++;
                                    if (*p == '=') p++;
                                    while (*p == ' ' || *p == '\t') p++;
                                    char val[256]; int vi = 0;
                                    if (*p == '"') { p++; while (*p && *p != '"' && vi < 250) val[vi++] = *p++; if (*p == '"') p++; }
                                    else { while (*p && *p != '\n' && *p != '\r' && vi < 250) val[vi++] = *p++; }
                                    val[vi] = 0;
                                    if (idx > 0 && idx <= MAX_TABS)
                                        snprintf(tab_dnames[idx-1], sizeof(tab_dnames[idx-1]), "%s", val);
                                }
                                while (*p && *p != '\n' && *p != '\r' && *p != '}') { if (*p == '{') sd++; p++; }
                            }
                            if (*p == '}') p++;
                        }
                        continue;
                    }

                    if (strncmp(p, "propertiesCreativeTab@minecraft", 31) == 0) {
                        p += 31;
                        while (*p == ' ' || *p == '\t') p++;
                        if (*p == '{') { p++;
                            int sd = 1;
                            while (*p && sd > 0) {
                                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
                                if (*p == '}') { sd--; if (!sd) break; continue; }
                                if (strncmp(p, "icon", 4) == 0 && (p[4] == ' ' || p[4] == '=')) {
                                    p += 4; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                                    int ii = 0;
                                    if (*p == '"') { p++; while (*p && *p != '"' && ii < 60) icon[ii++] = *p++; if (*p == '"') p++; }
                                    else { while (*p && *p != '\n' && *p != '\r' && *p != ' ' && ii < 60) icon[ii++] = *p++; }
                                    icon[ii] = 0; icon_set = 1; continue;
                                }
                                if (strncmp(p, "items", 5) == 0 && (p[5] == ' ' || p[5] == '=')) {
                                    p += 5; while (*p == ' ' || *p == '\t' || *p == '=') p++;
                                    while (*p && *p != '\n' && *p != '\r' && nitems < 64) {
                                        while (*p == ' ' || *p == '\t' || *p == ',') p++;
                                        if (*p == '\n' || *p == '\r') break;
                                        if (*p == '"') { p++; int ii = 0;
                                            while (*p && *p != '"' && ii < 60) items_list[nitems][ii++] = *p++;
                                            if (*p == '"') p++; items_list[nitems][ii] = 0; nitems++;
                                        } else { int ii = 0;
                                            while (*p && *p != ',' && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t' && ii < 60)
                                                items_list[nitems][ii++] = *p++;
                                            items_list[nitems][ii] = 0; nitems++;
                                        }
                                    }
                                    continue;
                                }
                                while (*p && *p != '\n' && *p != '\r') p++;
                            }
                            if (*p == '}') p++;
                        }
                        continue;
                    }
                    while (*p && *p != '\n' && *p != '\r') p++;
                }
                for (int i = 0; i < ntabs; i++) {
                    McCreativeTab tab;
                    memset(&tab, 0, sizeof(tab));
                    tab.name = sdup(tnames[i]);
                    if (icon_set) tab.icon = sdup(icon);
                    for (int j = 0; j < nitems; j++)
                        tab.items[tab.nitems++] = sdup(items_list[j]);
                    mc_add_tab(&tab);
                    if (tab_dnames[i][0] && mc_tab_count > 0) {
                        if (mc_tab_display_names[mc_tab_count - 1]) free(mc_tab_display_names[mc_tab_count - 1]);
                        mc_tab_display_names[mc_tab_count - 1] = sdup(tab_dnames[i]);
                    }
                }
                if (*p == '}') p++;
            }
        }
        goto end_line;

        scan_line: while (*p && *p != '\n' && *p != '\r') p++;
        end_line: continue;
    }
}

static char *mc_prop_val(const char *name) {
    for (int i = 0; i < mc_prop_count; i++)
        if (!strcmp(mc_props[i].name, name)) return mc_props[i].value;
    return NULL;
}

static char *mc_name_to_java(const char *name) {
    size_t len = strlen(name);
    char *out = malloc(len * 2 + 1);
    int j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') {
            if (i > 0) out[j++] = '_';
            out[j++] = c;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out[j++] = c - 32;
        else if (c == '-' || c == '_')
            out[j++] = '_';
    }
    out[j] = 0;
    return out;
}

static char *mc_upper_first(const char *s) {
    char *r = sdup(s);
    if (r[0] >= 'a' && r[0] <= 'z') r[0] -= 32;
    return r;
}

static char *mc_to_pascal(const char *s) {
    char *r = sdup(s);
    int j = 0, cap = 1;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '_') { cap = 1; continue; }
        if (cap && s[i] >= 'a' && s[i] <= 'z') { r[j++] = s[i] - 32; cap = 0; }
        else { r[j++] = s[i]; cap = 0; }
    }
    r[j] = 0;
    return r;
}

static void cg_java_block(FILE *f, ASTNode *n, int indent);
static void cg_java_stmt(FILE *f, ASTNode *n, int indent);
static void cg_java_expr(FILE *f, ASTNode *n);

static void cg_indent(FILE *f, int indent) {
    for (int i = 0; i < indent; i++) fprintf(f, "    ");
}

static void cg_java_block(FILE *f, ASTNode *n, int indent) {
    if (!n) return;
    if (n->type == NODE_BLOCK) {
        for (int i = 0; i < n->data.block.count; i++)
            cg_java_stmt(f, n->data.block.stmts[i], indent);
    } else {
        cg_java_stmt(f, n, indent);
    }
}

static void cg_java_expr(FILE *f, ASTNode *n) {
    if (!n) { fprintf(f, "0"); return; }
    switch (n->type) {
        case NODE_NUM: {
            if (n->data.num == (long)n->data.num)
                fprintf(f, "%ld", (long)n->data.num);
            else
                fprintf(f, "%.10gf", n->data.num);
            break;
        }
        case NODE_STR: {
            char *esc = cescape(n->data.str);
            fprintf(f, "\"%s\"", esc);
            free(esc);
            break;
        }
        case NODE_ID: {
            fprintf(f, "%s", n->data.id);
            break;
        }
        case NODE_BINOP: {
            int op = n->data.binop.op;
            if (op == TOK_PLUS) { cg_java_expr(f, n->data.binop.left); fprintf(f, "+"); cg_java_expr(f, n->data.binop.right); }
            else if (op == TOK_MINUS) { cg_java_expr(f, n->data.binop.left); fprintf(f, "-"); cg_java_expr(f, n->data.binop.right); }
            else if (op == TOK_STAR) { cg_java_expr(f, n->data.binop.left); fprintf(f, "*"); cg_java_expr(f, n->data.binop.right); }
            else if (op == TOK_SLASH) { cg_java_expr(f, n->data.binop.left); fprintf(f, "/"); cg_java_expr(f, n->data.binop.right); }
            else { cg_java_expr(f, n->data.binop.left); fprintf(f, ","); cg_java_expr(f, n->data.binop.right); }
            break;
        }
        case NODE_UNARY: {
            if (n->data.unary.op == TOK_MINUS) fprintf(f, "-");
            else if (n->data.unary.op == TOK_NOT) fprintf(f, "!");
            cg_java_expr(f, n->data.unary.operand);
            break;
        }
        case NODE_FUNC_CALL: {
            char *fn = n->data.func_call.name;
            char *lib = n->data.func_call.lib;
            if (lib && !strcmp(lib, "minecraft")) {
                if (!strcmp(fn, "applyEffect")) {
                    ASTNode **a = n->data.func_call.args;
                    int na = n->data.func_call.nargs;
                    fprintf(f, "player.addEffect(new MobEffectInstance(MobEffects.");
                    if (na > 1 && a[1]->type == NODE_STR)
                        fprintf(f, "%s", a[1]->data.str);
                    else fprintf(f, "NIGHT_VISION");
                    fprintf(f, ",");
                    if (na > 2 && a[2]->type == NODE_STR && !strcmp(a[2]->data.str, "infinite"))
                        fprintf(f, "Integer.MAX_VALUE");
                    else if (na > 2) cg_java_expr(f, a[2]);
                    else fprintf(f, "Integer.MAX_VALUE");
                    fprintf(f, ",");
                    if (na > 3) cg_java_expr(f, a[3]);
                    else fprintf(f, "255");
                    fprintf(f, ",false,true))");
                } else if (!strcmp(fn, "hurtPlayer")) {
                    ASTNode **a = n->data.func_call.args;
                    fprintf(f, "player.hurt(DamageSource.playerAttack(player),");
                    if (n->data.func_call.nargs > 1) cg_java_expr(f, a[1]);
                    else fprintf(f, "5");
                    fprintf(f, ".0f)");
                } else if (!strcmp(fn, "killPlayer")) {
                    fprintf(f, "player.kill()");
                } else {
                    goto default_func_call;
                }
            } else if (!strcmp(fn, "write")) {
                fprintf(f, "System.out.println(");
                for (int i = 0; i < n->data.func_call.nargs; i++) {
                    if (i > 0) fprintf(f, "+");
                    cg_java_expr(f, n->data.func_call.args[i]);
                }
                fprintf(f, ")");
            } else if (!strcmp(fn, "wait")) {
                fprintf(f, "try{Thread.sleep(");
                if (n->data.func_call.nargs >= 2) {
                    if (n->data.func_call.args[0]->type == NODE_NUM) {
                        double secs = n->data.func_call.args[0]->data.num;
                        fprintf(f, "%ldL", (long)(secs * 1000));
                    } else {
                        fprintf(f, "(long)(");
                        cg_java_expr(f, n->data.func_call.args[0]);
                        fprintf(f, "*1000)");
                    }
                } else if (n->data.func_call.nargs >= 1) {
                    if (n->data.func_call.args[0]->type == NODE_NUM) {
                        fprintf(f, "%ldL", (long)n->data.func_call.args[0]->data.num);
                    } else {
                        fprintf(f, "(long)(");
                        cg_java_expr(f, n->data.func_call.args[0]);
                        fprintf(f, "*1000)");
                    }
                } else fprintf(f, "1000L");
                fprintf(f, ");}catch(InterruptedException e){}");
            } else {
                default_func_call:
                fprintf(f, "%s(", fn);
                for (int i = 0; i < n->data.func_call.nargs; i++) {
                    if (i > 0) fprintf(f, ",");
                    cg_java_expr(f, n->data.func_call.args[i]);
                }
                fprintf(f, ")");
            }
            break;
        }
        case NODE_FUNCTION: {
            char *lib = n->data.funcall.lib;
            char *fn = n->data.funcall.args[0];
            if (lib && !strcmp(lib, "minecraft")) {
                if (!strcmp(fn, "applyEffect")) {
                    fprintf(f, "player.addEffect(new MobEffectInstance(MobEffects.%s,",
                        n->data.funcall.argc > 2 ? n->data.funcall.args[2] : "NIGHT_VISION");
                    if (n->data.funcall.argc > 3 && !strcmp(n->data.funcall.args[3], "infinite"))
                        fprintf(f, "Integer.MAX_VALUE");
                    else if (n->data.funcall.argc > 3)
                        fprintf(f, "%s", n->data.funcall.args[3]);
                    else
                        fprintf(f, "Integer.MAX_VALUE");
                    fprintf(f, ",");
                    if (n->data.funcall.argc > 4)
                        fprintf(f, "%s", n->data.funcall.args[4]);
                    else
                        fprintf(f, "255");
                    fprintf(f, ",false,true))");
                } else if (!strcmp(fn, "hurtPlayer")) {
                    fprintf(f, "player.hurt(DamageSource.playerAttack(player),%s",
                        n->data.funcall.argc > 2 ? n->data.funcall.args[2] : "5");
                    fprintf(f, ".0f)");
                } else if (!strcmp(fn, "killPlayer")) {
                    fprintf(f, "player.kill()");
                } else {
                    fprintf(f, "%s", fn);
                }
            } else if (lib && !strcmp(lib, "math") && (!strcmp(fn, "sleep") || !strcmp(fn, "wait"))) {
                fprintf(f, "try{Thread.sleep(");
                if (n->data.funcall.argc > 1) {
                    if (n->data.funcall.args[1][0] >= '0' && n->data.funcall.args[1][0] <= '9')
                        fprintf(f, "%sL", n->data.funcall.args[1]);
                    else
                        fprintf(f, "(long)(%s*1000)", n->data.funcall.args[1]);
                }
                fprintf(f, ");}catch(InterruptedException e){}");
            } else {
                fprintf(f, "%s", fn);
            }
            break;
        }
        case NODE_METHOD_CALL: {
            cg_java_expr(f, n->data.method_call.receiver);
            fprintf(f, ".%s(", n->data.method_call.method);
            for (int i = 0; i < n->data.method_call.argc; i++) {
                if (i > 0) fprintf(f, ",");
                cg_java_expr(f, n->data.method_call.args[i]);
            }
            fprintf(f, ")");
            break;
        }
        default: fprintf(f, "0"); break;
    }
}

static void cg_java_stmt(FILE *f, ASTNode *n, int indent) {
    if (!n) return;
    switch (n->type) {
        case NODE_EMPTY: break;
        case NODE_BLOCK: cg_java_block(f, n, indent); break;
        case NODE_ASSIGN: {
            cg_indent(f, indent);
            fprintf(f, "%s = ", n->data.assign.name);
            cg_java_expr(f, n->data.assign.value);
            fprintf(f, ";\n");
            break;
        }
        case NODE_PRINT: {
            cg_indent(f, indent);
            fprintf(f, "System.out.println(");
            for (int i = 0; i < n->data.print.count; i++) {
                if (i > 0) fprintf(f, "+");
                cg_java_expr(f, n->data.print.exprs[i]);
            }
            fprintf(f, ");\n");
            break;
        }
        case NODE_IF: {
            cg_indent(f, indent);
            fprintf(f, "if(");
            cg_java_expr(f, n->data.if_stmt.cond);
            fprintf(f, "){\n");
            cg_java_block(f, n->data.if_stmt.then, indent + 1);
            if (n->data.if_stmt.els) {
                if (((ASTNode*)n->data.if_stmt.els)->type == NODE_IF)
                    fprintf(f, "}else ");
                else
                    { cg_indent(f, indent); fprintf(f, "}else{\n"); }
                cg_java_block(f, (ASTNode*)n->data.if_stmt.els, indent + 1);
                if (((ASTNode*)n->data.if_stmt.els)->type != NODE_IF) {
                    cg_indent(f, indent); fprintf(f, "}\n");
                }
            } else {
                cg_indent(f, indent); fprintf(f, "}\n");
            }
            break;
        }
        case NODE_WHILE: {
            cg_indent(f, indent);
            fprintf(f, "while(");
            cg_java_expr(f, n->data.while_stmt.cond);
            fprintf(f, "){\n");
            cg_java_block(f, n->data.while_stmt.body, indent + 1);
            cg_indent(f, indent); fprintf(f, "}\n");
            break;
        }
        case NODE_FORTO: {
            cg_indent(f, indent);
            fprintf(f, "for(%s=", n->data.forto.var);
            cg_java_expr(f, n->data.forto.start);
            fprintf(f, ";%s<=", n->data.forto.var);
            cg_java_expr(f, n->data.forto.end);
            fprintf(f, ";%s++){\n", n->data.forto.var);
            cg_java_block(f, n->data.forto.body, indent + 1);
            cg_indent(f, indent); fprintf(f, "}\n");
            break;
        }
        case NODE_LOOP: {
            cg_indent(f, indent);
            fprintf(f, "while(true){\n");
            cg_java_block(f, n->data.loop.body, indent + 1);
            cg_indent(f, indent); fprintf(f, "}\n");
            break;
        }
        case NODE_FUNC_DEF: {
            if (n->data.func_def.lib && !strcmp(n->data.func_def.lib, "minecraft")) {
                char *fname = n->data.func_def.name;
                if (!strcmp(fname, "newItem") || !strcmp(fname, "newFood")) {
                    int is_food = !strcmp(fname, "newFood");
                    for (int i = 0; i < n->data.func_def.nparams; i++) {
                        if (mc_item_count >= MAX_ITEMS) break;
                        char *pname = n->data.func_def.params[i];
                        McItem *it = &mc_items[mc_item_count];
                        memset(it, 0, sizeof(McItem));
                        it->name = sdup(pname);
                        it->item_index = i + 1;
                        it->is_food = is_food;

                        char *lower = str_lower(pname);
                        if (strstr(lower, "_sword") || !strcmp(pname, "sword")) it->type = sdup("sword");
                        else if (strstr(lower, "_pickaxe") || !strcmp(pname, "pickaxe")) it->type = sdup("pickaxe");
                        else if (strstr(lower, "_axe") || !strcmp(pname, "axe")) it->type = sdup("axe");
                        else if (strstr(lower, "_shovel") || !strcmp(pname, "shovel")) it->type = sdup("shovel");
                        else if (strstr(lower, "_hoe") || !strcmp(pname, "hoe")) it->type = sdup("hoe");
                        else if (strstr(lower, "_helmet") || !strcmp(pname, "helmet")) it->type = sdup("helmet");
                        else if (strstr(lower, "_chestplate") || !strcmp(pname, "chestplate")) it->type = sdup("chestplate");
                        else if (strstr(lower, "_leggings") || !strcmp(pname, "leggings")) it->type = sdup("leggings");
                        else if (strstr(lower, "_boots") || !strcmp(pname, "boots")) it->type = sdup("boots");
                        else it->type = sdup("item");
                        it->filename = mc_to_pascal(pname);
                        ASTNode *body = n->data.func_def.body;
                        int has_names = 0;
                        if (body && body->type == NODE_BLOCK) {
                            for (int k = 0; k < body->data.block.count; k++) {
                                ASTNode *s2 = body->data.block.stmts[k];
                                if (s2->type == NODE_FUNC_DEF && s2->data.func_def.lib &&
                                    !strcmp(s2->data.func_def.lib, "minecraft") &&
                                    s2->data.func_def.name && !strcmp(s2->data.func_def.name, "itemNames"))
                                    has_names = 1;
                            }
                        }
                        if (has_names) {
                            if (mc_name_consumer < MAX_ITEMS && mc_item_display_names[mc_name_consumer])
                                it->display_name = sdup(mc_item_display_names[mc_name_consumer]);
                            mc_name_consumer++;
                        }

                        free(lower);
                        if (body && body->type == NODE_BLOCK) {
                            for (int j = 0; j < body->data.block.count; j++) {
                                ASTNode *s = body->data.block.stmts[j];
                                if (s->type == NODE_FUNC_DEF && s->data.func_def.lib &&
                                    !strcmp(s->data.func_def.lib, "minecraft")) {
                                    char *sfn = s->data.func_def.name;
                                    if (!strcmp(sfn, "propertiesItem")) {
                                        it->prop_block = s->data.func_def.body;
                                    } else if (!strcmp(sfn, "procedureItem")) {
                                        it->proc_block = s->data.func_def.body;
                                        if (s->data.func_def.nparams > 0)
                                            it->proc_event = sdup(s->data.func_def.params[0]);
                                    }
                                } else if (s->type == NODE_FUNC_CALL && s->data.func_call.lib
                                    && !strcmp(s->data.func_call.lib, "minecraft")
                                    && !strcmp(s->data.func_call.name, "foodEffect")) {
                                    it->has_effects = 1;
                                }
                            }
                        }
                        mc_item_count++;
                    }
                } else if (!strcmp(fname, "newBlock")) {
                    for (int i = 0; i < n->data.func_def.nparams; i++) {
                        if (mc_block_count >= MAX_BLOCKS) break;
                        char *pname = n->data.func_def.params[i];
                        McBlock *bl = &mc_blocks[mc_block_count];
                        memset(bl, 0, sizeof(McBlock));
                        bl->name = sdup(pname);
                        bl->item_index = i + 1;
                        bl->filename = mc_to_pascal(pname);
                        ASTNode *body = n->data.func_def.body;
                        int has_names = 0;
                        if (body && body->type == NODE_BLOCK) {
                            for (int k = 0; k < body->data.block.count; k++) {
                                ASTNode *s2 = body->data.block.stmts[k];
                                if (s2->type == NODE_FUNC_DEF && s2->data.func_def.lib &&
                                    !strcmp(s2->data.func_def.lib, "minecraft") &&
                                    s2->data.func_def.name && !strcmp(s2->data.func_def.name, "itemNames"))
                                    has_names = 1;
                            }
                        }
                        if (has_names) {
                            if (mc_name_consumer < MAX_ITEMS && mc_item_display_names[mc_name_consumer])
                                bl->display_name = sdup(mc_item_display_names[mc_name_consumer]);
                            mc_name_consumer++;
                        }
                        if (body && body->type == NODE_BLOCK) {
                            for (int j = 0; j < body->data.block.count; j++) {
                                ASTNode *s = body->data.block.stmts[j];
                                if (s->type == NODE_FUNC_DEF && s->data.func_def.lib &&
                                    !strcmp(s->data.func_def.lib, "minecraft")) {
                                    char *sfn = s->data.func_def.name;
                                    if (!strcmp(sfn, "propertiesBlock")) {
                                        bl->prop_block = s->data.func_def.body;
                                    } else if (!strcmp(sfn, "procedureBlock")) {
                                        bl->proc_block = s->data.func_def.body;
                                        if (s->data.func_def.nparams > 0)
                                            bl->proc_event = sdup(s->data.func_def.params[0]);
                                    }
                                }
                            }
                        }
                        mc_block_count++;
                    }
                }
                break;
            }
            cg_indent(f, indent);
            fprintf(f, "public static void %s(", n->data.func_def.name);
            for (int i = 0; i < n->data.func_def.nparams; i++) {
                if (i > 0) fprintf(f, ",");
                fprintf(f, "Value p%d", i);
            }
            fprintf(f, "){\n");
            cg_java_block(f, n->data.func_def.body, indent + 1);
            cg_indent(f, indent); fprintf(f, "}\n");
            break;
        }
        case NODE_FUNC_CALL: {
            cg_indent(f, indent);
            cg_java_expr(f, n);
            fprintf(f, ";\n");
            break;
        }
        case NODE_EXIT: {
            cg_indent(f, indent);
            fprintf(f, "return;\n");
            break;
        }
        default: break;
    }
}

static int is_vanilla_tier(const char *name) {
    const char *vanilla[] = {"WOOD", "STONE", "IRON", "GOLD", "DIAMOND", "NETHERITE"};
    char *upper = mc_name_to_java(name);
    if (!upper) return 0;
    int r = 0;
    for (int i = 0; i < 6; i++) if (!strcmp(upper, vanilla[i])) { r = 1; break; }
    free(upper);
    return r;
}

static void cg_to_java_item_class(FILE *f, McItem *item) {
    char *jname = item->filename;
    fprintf(f, "package com.%s.%s;\n\n", mc_mod_id, mc_mod_id);
    fprintf(f, "import net.minecraft.world.item.*;\n");
    fprintf(f, "import net.minecraft.world.effect.MobEffectInstance;\n");
    fprintf(f, "import net.minecraft.world.effect.MobEffects;\n");
    fprintf(f, "import net.minecraft.world.entity.player.Player;\n");
    fprintf(f, "import net.minecraft.world.entity.LivingEntity;\n");
    fprintf(f, "import net.minecraft.world.damagesource.DamageSource;\n");
    fprintf(f, "import net.minecraft.world.InteractionResultHolder;\n");
    fprintf(f, "import net.minecraft.world.InteractionHand;\n");
    fprintf(f, "import net.minecraft.world.level.Level;\n");
    fprintf(f, "import net.minecraft.world.item.ItemStack;\n");
    fprintf(f, "import net.minecraftforge.registries.DeferredRegister;\n");
    fprintf(f, "import net.minecraftforge.registries.ForgeRegistries;\n");
    fprintf(f, "import net.minecraftforge.registries.RegistryObject;\n");
    fprintf(f, "import com.%s.%s.ModItems;\n", mc_mod_id, mc_mod_id);
    int has_custom_tier = 0;
    char *tier = "WOOD", *upper_tier = sdup("WOOD"), *tier_prefix = "Tiers.";
    int attack_dmg = 3;
    float attack_speed = -2.0f;
    int immune_fire = 0;
    int durability = 0, stacks_to = 64, hunger = 0, always_edible = 0;
    float saturation = 0;
    if (item->prop_block && item->prop_block->type == NODE_BLOCK) {
        for (int i = 0; i < item->prop_block->data.block.count; i++) {
            ASTNode *s = item->prop_block->data.block.stmts[i];
            if (s->type == NODE_ASSIGN) {
                char *pn = s->data.assign.name;
                if (!strcmp(pn, "tier") && s->data.assign.value->type == NODE_STR)
                    tier = mc_upper_first(s->data.assign.value->data.str);
                else if (!strcmp(pn, "attackDamage") && s->data.assign.value->type == NODE_NUM)
                    attack_dmg = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "attackSpeed") && s->data.assign.value->type == NODE_NUM)
                    attack_speed = (float)s->data.assign.value->data.num;
                else if (!strcmp(pn, "isImmuneToLava") && s->data.assign.value->type == NODE_NUM)
                    immune_fire = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "durability") && s->data.assign.value->type == NODE_NUM)
                    durability = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "stacksTo") && s->data.assign.value->type == NODE_NUM)
                    stacks_to = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "hunger") && s->data.assign.value->type == NODE_NUM)
                    hunger = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "saturation") && s->data.assign.value->type == NODE_NUM)
                    saturation = (float)s->data.assign.value->data.num;
                else if (!strcmp(pn, "alwaysEdible") && s->data.assign.value->type == NODE_NUM)
                    always_edible = (int)s->data.assign.value->data.num;
            }
        }
    }

    if (strcmp(tier, "WOOD")) {
        char *tmp = mc_name_to_java(tier);
        free(upper_tier);
        upper_tier = tmp;
        tier_prefix = is_vanilla_tier(tier) ? "Tiers." : "ModToolTiers.";
    }
    int is_armor = !strcmp(item->type, "helmet") || !strcmp(item->type, "chestplate")
        || !strcmp(item->type, "leggings") || !strcmp(item->type, "boots");

    if (!strcmp(item->type, "sword")) {
        fprintf(f, "public class %s extends SwordItem {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(%s%s, %d, %ff, new Item.Properties()", tier_prefix, upper_tier, attack_dmg, (double)attack_speed);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ");\n    }\n");
    } else if (!strcmp(item->type, "pickaxe")) {
        fprintf(f, "public class %s extends PickaxeItem {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(%s%s, %d, %ff, new Item.Properties()", tier_prefix, upper_tier, attack_dmg, (double)attack_speed);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ");\n    }\n");
    } else if (!strcmp(item->type, "axe")) {
        fprintf(f, "public class %s extends AxeItem {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(%s%s, %ff, %ff, new Item.Properties()", tier_prefix, upper_tier, (double)attack_dmg, (double)attack_speed);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ");\n    }\n");
    } else if (!strcmp(item->type, "shovel")) {
        fprintf(f, "public class %s extends ShovelItem {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(%s%s, %ff, %ff, new Item.Properties()", tier_prefix, upper_tier, (double)attack_dmg, (double)attack_speed);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ");\n    }\n");
    } else if (!strcmp(item->type, "hoe")) {
        fprintf(f, "public class %s extends HoeItem {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(%s%s, %d, %ff, new Item.Properties()", tier_prefix, upper_tier, attack_dmg, (double)attack_speed);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ");\n    }\n");
    } else if (is_armor) {
        fprintf(f, "public class %s extends ArmorItem {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        char *armor_type = !strcmp(item->type, "helmet") ? "ArmorItem.Type.HELMET"
            : !strcmp(item->type, "chestplate") ? "ArmorItem.Type.CHESTPLATE"
            : !strcmp(item->type, "leggings") ? "ArmorItem.Type.LEGGINGS" : "ArmorItem.Type.BOOTS";
        fprintf(f, "        super(ModArmorMaterials.%s, %s, new Item.Properties()",
            mc_name_to_java(tier), armor_type);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ");\n    }\n");
    } else if (item->is_food) {
        fprintf(f, "public class %s extends Item {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(new Item.Properties()");
        if (durability > 0) fprintf(f, ".durability(%d)", durability);
        fprintf(f, ".food(new FoodProperties.Builder()\n");
        if (hunger > 0) fprintf(f, "                .nutrition(%d)\n", hunger);
        if (saturation > 0) fprintf(f, "                .saturationMod(%ff)\n", saturation);
        if (always_edible) fprintf(f, "                .alwaysEat()\n");
        fprintf(f, "                .build()));\n");
        fprintf(f, "    }\n");
    } else {
        fprintf(f, "public class %s extends Item {\n", jname);
        fprintf(f, "    public %s() {\n", jname);
        fprintf(f, "        super(new Item.Properties()");
        if (durability > 0) fprintf(f, ".durability(%d)", durability);
        if (immune_fire) fprintf(f, ".fireResistant()");
        fprintf(f, ".stacksTo(%d)", stacks_to);
        fprintf(f, ");\n    }\n");
    }

    if (item->proc_block) {
        fprintf(f, "    @Override\n");
        if (item->proc_event && strstr(item->proc_event, "RightClick"))
            fprintf(f, "    public InteractionResultHolder<ItemStack> use(Level level, Player player, InteractionHand hand) {\n");
        else if (item->proc_event && strstr(item->proc_event, "HitEntity"))
            fprintf(f, "    public boolean hurtEnemy(ItemStack stack, LivingEntity target, LivingEntity attacker) {\n");
        else if (item->proc_event && strstr(item->proc_event, "Crafted"))
            fprintf(f, "    public void onCraftedBy(ItemStack stack, Level level, Player player) {\n");
        else
            fprintf(f, "    public InteractionResultHolder<ItemStack> use(Level level, Player player, InteractionHand hand) {\n");
        cg_java_block(f, item->proc_block, 2);
        if (item->proc_event && strstr(item->proc_event, "RightClick"))
            fprintf(f, "        return super.use(level, player, hand);\n");
        else if (item->proc_event && strstr(item->proc_event, "HitEntity"))
            fprintf(f, "        return true;\n");
        else if (item->proc_event && strstr(item->proc_event, "Crafted")) {}
        else
            fprintf(f, "        return super.use(level, player, hand);\n");
        fprintf(f, "    }\n");
    }

    fprintf(f, "}\n");
}

static void cg_to_java_block_class(FILE *f, McBlock *bl) {
    char *jname = bl->filename;
    char *matcol = "MapColor.STONE";
    float hardness = 3.0f, resistance = 3.0f;
    int requires_tool = 0, light = 0;
    char *sound = "SoundType.STONE";

    if (bl->prop_block && bl->prop_block->type == NODE_BLOCK) {
        for (int j = 0; j < bl->prop_block->data.block.count; j++) {
            ASTNode *s = bl->prop_block->data.block.stmts[j];
            if (s->type == NODE_ASSIGN) {
                char *pn = s->data.assign.name;
                if (!strcmp(pn, "material") && s->data.assign.value->type == NODE_STR) {
                    if (!strcmp(s->data.assign.value->data.str, "metal")) matcol = "MapColor.METAL";
                    else if (!strcmp(s->data.assign.value->data.str, "wood")) matcol = "MapColor.WOOD";
                    else if (!strcmp(s->data.assign.value->data.str, "glass")) { matcol = "MapColor.NONE"; sound = "SoundType.GLASS"; }
                    else if (!strcmp(s->data.assign.value->data.str, "stone")) matcol = "MapColor.STONE";
                    else if (!strcmp(s->data.assign.value->data.str, "dirt")) matcol = "MapColor.DIRT";
                    else if (!strcmp(s->data.assign.value->data.str, "plant")) matcol = "MapColor.PLANT";
                    else if (!strcmp(s->data.assign.value->data.str, "snow")) matcol = "MapColor.SNOW";
                } else if (!strcmp(pn, "hardness") && s->data.assign.value->type == NODE_NUM)
                    hardness = (float)s->data.assign.value->data.num;
                else if (!strcmp(pn, "resistance") && s->data.assign.value->type == NODE_NUM)
                    resistance = (float)s->data.assign.value->data.num;
                else if (!strcmp(pn, "requiresTool") && s->data.assign.value->type == NODE_NUM)
                    requires_tool = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "light") && s->data.assign.value->type == NODE_NUM)
                    light = (int)s->data.assign.value->data.num;
                else if (!strcmp(pn, "sound") && s->data.assign.value->type == NODE_STR) {
                    if (!strcmp(s->data.assign.value->data.str, "metal")) sound = "SoundType.METAL";
                    else if (!strcmp(s->data.assign.value->data.str, "wood")) sound = "SoundType.WOOD";
                    else if (!strcmp(s->data.assign.value->data.str, "stone")) sound = "SoundType.STONE";
                    else if (!strcmp(s->data.assign.value->data.str, "glass")) sound = "SoundType.GLASS";
                    else if (!strcmp(s->data.assign.value->data.str, "deepslate")) sound = "SoundType.DEEPSLATE";
                    else if (!strcmp(s->data.assign.value->data.str, "gravel")) sound = "SoundType.GRAVEL";
                    else if (!strcmp(s->data.assign.value->data.str, "snow")) sound = "SoundType.SNOW";
                }
            }
        }
    }

    fprintf(f, "package com.%s.%s;\n\n", mc_mod_id, mc_mod_id);
    fprintf(f, "import net.minecraft.core.BlockPos;\n");
    fprintf(f, "import net.minecraft.world.level.Level;\n");
    fprintf(f, "import net.minecraft.world.level.block.Block;\n");
    fprintf(f, "import net.minecraft.world.level.block.state.BlockBehaviour;\n");
    fprintf(f, "import net.minecraft.world.level.block.state.BlockState;\n");
    fprintf(f, "import net.minecraft.world.level.material.MapColor;\n");
    fprintf(f, "import net.minecraft.world.level.block.SoundType;\n\n");
    fprintf(f, "public class %s extends Block {\n", jname);
    fprintf(f, "    public %s() {\n", jname);
    fprintf(f, "        super(BlockBehaviour.Properties.of().mapColor(%s)\n", matcol);
    fprintf(f, "            .strength(%ff, %ff)\n", hardness, resistance);
    if (requires_tool) fprintf(f, "            .requiresCorrectToolForDrops()\n");
    if (light > 0) fprintf(f, "            .lightLevel(s -> %d)\n", light);
    fprintf(f, "            .sound(%s));\n", sound);
    fprintf(f, "    }\n");

    if (bl->proc_block) {
        if (bl->proc_event && strstr(bl->proc_event, "BlockPlaced"))
            fprintf(f, "    @Override\n    public void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean moved) {\n");
        if (bl->proc_event && strstr(bl->proc_event, "BlockPlaced")) {
            cg_java_block(f, bl->proc_block, 2);
            fprintf(f, "        super.onPlace(state, level, pos, oldState, moved);\n    }\n");
        }
    }

    fprintf(f, "}\n");
}

static void cg_to_java_item_registry(FILE *f) {
    if (!mc_item_count && !mc_block_count) return;
    char pkg[256];
    snprintf(pkg, sizeof(pkg), "com.%s.%s", mc_mod_id, mc_mod_id);
    fprintf(f, "package %s;\n\n", pkg);
    fprintf(f, "import net.minecraft.world.item.*;\n");
    fprintf(f, "import net.minecraft.world.food.FoodProperties;\n");
    fprintf(f, "import net.minecraft.world.effect.MobEffectInstance;\n");
    fprintf(f, "import net.minecraftforge.registries.DeferredRegister;\n");
    fprintf(f, "import net.minecraftforge.registries.ForgeRegistries;\n");
    fprintf(f, "import net.minecraftforge.registries.RegistryObject;\n");
    fprintf(f, "import net.minecraftforge.fml.common.Mod;\n");
    fprintf(f, "import net.minecraftforge.eventbus.api.IEventBus;\n");
    fprintf(f, "import net.minecraftforge.fml.javafmlmod.FMLJavaModLoadingContext;\n\n");

    fprintf(f, "public class ModItems {\n");
    fprintf(f, "    public static final DeferredRegister<Item> ITEMS = DeferredRegister.create(ForgeRegistries.ITEMS, \"%s\");\n\n", mc_mod_id);

    for (int i = 0; i < mc_item_count; i++) {
        McItem *it = &mc_items[i];
        char *upper = mc_name_to_java(it->name);
        char *jc = mc_upper_first(it->type);
        fprintf(f, "    public static final RegistryObject<");
        if (!strcmp(it->type, "sword")) fprintf(f, "SwordItem");
        else if (!strcmp(it->type, "pickaxe")) fprintf(f, "PickaxeItem");
        else if (!strcmp(it->type, "axe")) fprintf(f, "AxeItem");
        else if (!strcmp(it->type, "shovel")) fprintf(f, "ShovelItem");
        else if (!strcmp(it->type, "hoe")) fprintf(f, "HoeItem");
        else if (!strcmp(it->type, "helmet") || !strcmp(it->type, "chestplate")
            || !strcmp(it->type, "leggings") || !strcmp(it->type, "boots")) fprintf(f, "ArmorItem");
        else fprintf(f, "Item");

        fprintf(f, "> %s = ITEMS.register(\"%s\",\n", upper, it->name);
        fprintf(f, "        () -> new %s());\n\n", it->filename);
        free(upper); free(jc);
    }

    for (int i = 0; i < mc_block_count; i++) {
        char *upper = mc_name_to_java(mc_blocks[i].name);
        fprintf(f, "    public static final RegistryObject<BlockItem> %s_ITEM = ITEMS.register(\"%s\",\n", upper, mc_blocks[i].name);
        fprintf(f, "        () -> new BlockItem(ModBlocks.%s.get(), new Item.Properties()));\n\n", upper);
        free(upper);
    }

    fprintf(f, "    public static void register(IEventBus bus) {\n");
    fprintf(f, "        ITEMS.register(bus);\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
}

static void cg_to_java_block_registry(FILE *f) {
    if (!mc_block_count) return;
    char pkg[256];
    snprintf(pkg, sizeof(pkg), "com.%s.%s", mc_mod_id, mc_mod_id);
    fprintf(f, "package %s;\n\n", pkg);
    fprintf(f, "import net.minecraft.world.level.block.*;\n");
    fprintf(f, "import net.minecraft.world.level.block.state.BlockBehaviour;\n");
    fprintf(f, "import net.minecraft.world.level.material.MapColor;\n");
    fprintf(f, "import net.minecraft.world.level.block.SoundType;\n");
    fprintf(f, "import net.minecraftforge.registries.DeferredRegister;\n");
    fprintf(f, "import net.minecraftforge.registries.ForgeRegistries;\n");
    fprintf(f, "import net.minecraftforge.registries.RegistryObject;\n");
    fprintf(f, "import net.minecraftforge.eventbus.api.IEventBus;\n\n");

    fprintf(f, "public class ModBlocks {\n");
    fprintf(f, "    public static final DeferredRegister<Block> BLOCKS = DeferredRegister.create(ForgeRegistries.BLOCKS, \"%s\");\n\n", mc_mod_id);

    for (int i = 0; i < mc_block_count; i++) {
        McBlock *bl = &mc_blocks[i];
        char *upper = mc_name_to_java(bl->name);
        char *matcol = "MapColor.STONE";
        float hardness = 3.0f, resistance = 3.0f;
        int requires_tool = 0, light = 0;
        char *sound = "SoundType.STONE";

        if (bl->prop_block && bl->prop_block->type == NODE_BLOCK) {
            for (int j = 0; j < bl->prop_block->data.block.count; j++) {
                ASTNode *s = bl->prop_block->data.block.stmts[j];
                if (s->type == NODE_ASSIGN) {
                    char *pn = s->data.assign.name;
                    if (!strcmp(pn, "material") && s->data.assign.value->type == NODE_STR) {
                        if (!strcmp(s->data.assign.value->data.str, "metal")) matcol = "MapColor.METAL";
                        else if (!strcmp(s->data.assign.value->data.str, "wood")) matcol = "MapColor.WOOD";
                        else if (!strcmp(s->data.assign.value->data.str, "glass")) { matcol = "MapColor.NONE"; sound = "SoundType.GLASS"; }
                        else if (!strcmp(s->data.assign.value->data.str, "stone")) matcol = "MapColor.STONE";
                        else if (!strcmp(s->data.assign.value->data.str, "dirt")) matcol = "MapColor.DIRT";
                        else if (!strcmp(s->data.assign.value->data.str, "plant")) matcol = "MapColor.PLANT";
                        else if (!strcmp(s->data.assign.value->data.str, "snow")) matcol = "MapColor.SNOW";
                    } else if (!strcmp(pn, "hardness") && s->data.assign.value->type == NODE_NUM)
                        hardness = (float)s->data.assign.value->data.num;
                    else if (!strcmp(pn, "resistance") && s->data.assign.value->type == NODE_NUM)
                        resistance = (float)s->data.assign.value->data.num;
                    else if (!strcmp(pn, "requiresTool") && s->data.assign.value->type == NODE_NUM)
                        requires_tool = (int)s->data.assign.value->data.num;
                    else if (!strcmp(pn, "light") && s->data.assign.value->type == NODE_NUM)
                        light = (int)s->data.assign.value->data.num;
                    else if (!strcmp(pn, "sound") && s->data.assign.value->type == NODE_STR) {
                        if (!strcmp(s->data.assign.value->data.str, "metal")) sound = "SoundType.METAL";
                        else if (!strcmp(s->data.assign.value->data.str, "wood")) sound = "SoundType.WOOD";
                        else if (!strcmp(s->data.assign.value->data.str, "stone")) sound = "SoundType.STONE";
                        else if (!strcmp(s->data.assign.value->data.str, "glass")) sound = "SoundType.GLASS";
                        else if (!strcmp(s->data.assign.value->data.str, "deepslate")) sound = "SoundType.DEEPSLATE";
                        else if (!strcmp(s->data.assign.value->data.str, "gravel")) sound = "SoundType.GRAVEL";
                        else if (!strcmp(s->data.assign.value->data.str, "snow")) sound = "SoundType.SNOW";
                    }
                }
            }
        }
        fprintf(f, "    public static final RegistryObject<Block> %s = BLOCKS.register(\"%s\",\n", upper, bl->name);
        if (bl->proc_block) {
            fprintf(f, "        %s::new);\n\n", bl->filename);
        } else {
            fprintf(f, "        () -> new Block(BlockBehaviour.Properties.of().mapColor(%s)\n", matcol);
            fprintf(f, "            .strength(%ff, %ff)\n", hardness, resistance);
            if (requires_tool) fprintf(f, "            .requiresCorrectToolForDrops()\n");
            if (light > 0) fprintf(f, "            .lightLevel(s -> %d)\n", light);
            fprintf(f, "            .sound(%s)));\n\n", sound);
        }
        free(upper);
    }

    fprintf(f, "    public static void register(IEventBus bus) {\n");
    fprintf(f, "        BLOCKS.register(bus);\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
}

static const char *mc_strip_modid(const char *name) {
    size_t mlen = strlen(mc_mod_id);
    if (strncmp(name, mc_mod_id, mlen) == 0 && name[mlen] == ':')
        return name + mlen + 1;
    return name;
}

static void cg_to_java_creative_tabs(FILE *f) {
    if (!mc_tab_count) return;
    char pkg[256];
    snprintf(pkg, sizeof(pkg), "com.%s.%s", mc_mod_id, mc_mod_id);
    fprintf(f, "package %s;\n\n", pkg);
    fprintf(f, "import net.minecraft.core.registries.Registries;\n");
    fprintf(f, "import net.minecraft.network.chat.Component;\n");
    fprintf(f, "import net.minecraft.world.item.CreativeModeTab;\n");
    fprintf(f, "import net.minecraft.world.item.ItemStack;\n");
    fprintf(f, "import net.minecraftforge.registries.DeferredRegister;\n");
    fprintf(f, "import net.minecraftforge.registries.RegistryObject;\n");
    fprintf(f, "import net.minecraftforge.eventbus.api.IEventBus;\n\n");

    fprintf(f, "public class ModCreativeTabs {\n");
    fprintf(f, "    public static final DeferredRegister<CreativeModeTab> TABS = DeferredRegister.create(Registries.CREATIVE_MODE_TAB, \"%s\");\n\n", mc_mod_id);

    for (int i = 0; i < mc_tab_count; i++) {
        McCreativeTab *t = &mc_tabs[i];
        char *upper = mc_name_to_java(t->name);
        fprintf(f, "    public static final RegistryObject<CreativeModeTab> %s = TABS.register(\"%s\",\n", upper, t->name);
        fprintf(f, "        () -> CreativeModeTab.builder()\n");
        if (t->icon) {
            const char *icon_s = mc_strip_modid(t->icon);
            char *icon_upper = mc_name_to_java(icon_s);
            int is_block = 0;
            for (int k = 0; k < mc_block_count; k++)
                if (!strcmp(icon_s, mc_blocks[k].name)) { is_block = 1; break; }
            fprintf(f, "            .icon(() -> new ItemStack(ModItems.%s%s.get()))\n", icon_upper, is_block ? "_ITEM" : "");
            free(icon_upper);
        }
        fprintf(f, "            .title(Component.translatable(\"creativetab.%s.%s\"))\n", mc_mod_id, t->name);
        fprintf(f, "            .displayItems((params, output) -> {\n");
        for (int j = 0; j < t->nitems; j++) {
            const char *item_s = mc_strip_modid(t->items[j]);
            char *iu = mc_name_to_java(item_s);
            int is_block = 0;
            for (int k = 0; k < mc_block_count; k++)
                if (!strcmp(item_s, mc_blocks[k].name)) { is_block = 1; break; }
            fprintf(f, "                output.accept(ModItems.%s%s.get());\n", iu, is_block ? "_ITEM" : "");
            free(iu);
        }
        fprintf(f, "            })\n");
        fprintf(f, "            .build());\n\n");
        free(upper);
    }

    fprintf(f, "    public static void register(IEventBus bus) {\n");
    fprintf(f, "        TABS.register(bus);\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
}

static void cg_to_java_armor_materials(FILE *f) {
    if (!mc_armor_material_count) return;
    char pkg[256];
    snprintf(pkg, sizeof(pkg), "com.%s.%s", mc_mod_id, mc_mod_id);
    fprintf(f, "package %s;\n\n", pkg);
    fprintf(f, "import net.minecraft.sounds.SoundEvent;\n");
    fprintf(f, "import net.minecraft.sounds.SoundEvents;\n");
    fprintf(f, "import net.minecraft.world.item.ArmorItem;\n");
    fprintf(f, "import net.minecraft.world.item.ArmorMaterial;\n");
    fprintf(f, "import net.minecraft.world.item.crafting.Ingredient;\n");
    fprintf(f, "import net.minecraftforge.registries.ForgeRegistries;\n");
    fprintf(f, "import java.util.function.Supplier;\n\n");

    fprintf(f, "public enum ModArmorMaterials implements ArmorMaterial {\n");
    for (int i = 0; i < mc_armor_material_count; i++) {
        McArmorMaterial *m = &mc_armor_materials[i];
        char *sn = m->equip_sound && !strcmp(m->equip_sound, "diamond") ? "SoundEvents.ARMOR_EQUIP_DIAMOND"
            : m->equip_sound && !strcmp(m->equip_sound, "iron") ? "SoundEvents.ARMOR_EQUIP_IRON"
            : m->equip_sound && !strcmp(m->equip_sound, "chain") ? "SoundEvents.ARMOR_EQUIP_CHAIN"
            : m->equip_sound && !strcmp(m->equip_sound, "gold") ? "SoundEvents.ARMOR_EQUIP_GOLD"
            : m->equip_sound && !strcmp(m->equip_sound, "leather") ? "SoundEvents.ARMOR_EQUIP_LEATHER"
            : "SoundEvents.ARMOR_EQUIP_DIAMOND";
        fprintf(f, "    %s(\"%s\", %d, new int[]{%d, %d, %d, %d}, 15, %s, %ff, %ff,\n",
            mc_name_to_java(m->name), m->name, m->durability,
            m->protection[0], m->protection[1], m->protection[2], m->protection[3],
            sn, m->toughness, m->knockback);
        fprintf(f, "        () -> Ingredient.of(ModItems.%s.get())),\n", mc_name_to_java(m->repair_item));
    }
    fprintf(f, ";\n    // ... methods omitted for brevity (use IDE generate or full template)\n");
    fprintf(f, "}\n");
}

static void cg_to_java_tool_tiers(FILE *f) {
    if (!mc_tool_tier_count) return;
    char pkg[256];
    snprintf(pkg, sizeof(pkg), "com.%s.%s", mc_mod_id, mc_mod_id);
    fprintf(f, "package %s;\n\n", pkg);
    fprintf(f, "import net.minecraft.world.item.Tier;\n");
    fprintf(f, "import net.minecraft.world.item.crafting.Ingredient;\n");
    fprintf(f, "import java.util.function.Supplier;\n\n");

    fprintf(f, "public class ModToolTiers {\n");
    for (int i = 0; i < mc_tool_tier_count; i++) {
        McToolTier *t = &mc_tool_tiers[i];
        char *upper = mc_name_to_java(t->name);
        fprintf(f, "    public static final Tier %s = new Tier() {\n", upper);
        fprintf(f, "        public int getLevel() { return %d; }\n", t->level);
        fprintf(f, "        public int getUses() { return %d; }\n", t->uses);
        fprintf(f, "        public float getSpeed() { return %ff; }\n", t->speed);
        fprintf(f, "        public float getAttackDamageBonus() { return %ff; }\n", t->damage);
        fprintf(f, "        public int getEnchantmentValue() { return %d; }\n", t->enchantability);
        fprintf(f, "        public Ingredient getRepairIngredient() { return Ingredient.of(ModItems.%s.get()); }\n", mc_name_to_java(t->repair_item));
        fprintf(f, "    };\n");
    }
    fprintf(f, "}\n");
}

static void cg_to_java_main(FILE *f) {
    char pkg[256];
    snprintf(pkg, sizeof(pkg), "com.%s.%s", mc_mod_id, mc_mod_id);
    fprintf(f, "package %s;\n\n", pkg);
    fprintf(f, "import net.minecraftforge.common.MinecraftForge;\n");
    fprintf(f, "import net.minecraftforge.eventbus.api.IEventBus;\n");
    fprintf(f, "import net.minecraftforge.fml.common.Mod;\n");
    fprintf(f, "import net.minecraftforge.fml.javafmlmod.FMLJavaModLoadingContext;\n\n");

    fprintf(f, "@Mod(\"%s\")\n", mc_mod_id);
    fprintf(f, "public class ModMain {\n");
    fprintf(f, "    public static final String MODID = \"%s\";\n\n", mc_mod_id);
    fprintf(f, "    public ModMain() {\n");
    fprintf(f, "        IEventBus bus = FMLJavaModLoadingContext.get().getModEventBus();\n");
    if (mc_item_count || mc_block_count) fprintf(f, "        ModItems.ITEMS.register(bus);\n");
    if (mc_block_count) fprintf(f, "        ModBlocks.BLOCKS.register(bus);\n");
    if (mc_tab_count) fprintf(f, "        ModCreativeTabs.TABS.register(bus);\n");
    fprintf(f, "        MinecraftForge.EVENT_BUS.register(this);\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");
}

static void cg_to_gradle_properties(FILE *f) {
    for (int i = 0; i < mc_prop_count; i++) {
        char *name = mc_props[i].name;
        size_t nlen = strlen(name);
        if (nlen > 0 && name[0] == '_') continue;
        for (size_t j = 0; j < nlen; j++) {
            char c = name[j];
            if (c >= 'A' && c <= 'Z') {
                int prev_lower = (j > 0 && name[j-1] >= 'a' && name[j-1] <= 'z');
                int next_lower = (j + 1 < nlen && name[j+1] >= 'a' && name[j+1] <= 'z');
                if (prev_lower || (next_lower && j > 0)) fprintf(f, "_");
                fprintf(f, "%c", c + 32);
            } else {
                fprintf(f, "%c", c);
            }
        }
        fprintf(f, "=%s\n", mc_props[i].value);
    }
}

static void cg_to_registry_or_proc_file(ASTNode *prog) {
    char *mv = mc_prop_val("modID");
    if (mv) {
        char *l = str_lower(mv);
        snprintf(mc_mod_id, sizeof(mc_mod_id), "%s", l);
        free(l);
    } else {
        snprintf(mc_mod_id, sizeof(mc_mod_id), "example_mod");
    }

    for (int i = 0; i < prog->data.block.count; i++) {
        ASTNode *s = prog->data.block.stmts[i];
        if (s->type == NODE_FUNC_DEF && s->data.func_def.lib &&
            !strcmp(s->data.func_def.lib, "minecraft")) {
            cg_java_stmt(NULL, s, 0);
        }
    }

    if (mc_output_filename[0] && (mc_item_count > 0 || mc_block_count > 0)) {
        char *dot = strrchr(mc_output_filename, '.');
        if (dot && !strcmp(dot, ".java")) {
            size_t nlen = dot - mc_output_filename;
            for (int i = 0; i < mc_item_count; i++) {
                free(mc_items[i].filename);
                mc_items[i].filename = sdupn(mc_output_filename, nlen);
            }
        }
    }
}

static void cg_item_model_json(const char *name, const char *type, int idx) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/assets/%s/models/item/%s.json",
        mc_mod_pkg, mc_mod_id, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    McModel *m = (idx >= 0 && idx < MAX_ITEMS && mc_item_models[idx].parent[0])
        ? &mc_item_models[idx] : NULL;
    if (m) {
        fprintf(f, "{\n  \"parent\": \"item/%s\",\n  \"textures\": {\n", m->parent);
        for (int i = 0; i < m->ntex; i++) {
            const char *tv = m->tex_vals[i];
            int has_sep = strchr(tv, ':') || strchr(tv, '/');
            char tbuf[128];
            if (!has_sep) snprintf(tbuf, sizeof(tbuf), "%s:item/%s", mc_mod_id, tv);
            else snprintf(tbuf, sizeof(tbuf), "%s", tv);
            fprintf(f, "    \"%s\": \"%s\"%s\n", m->tex_keys[i], tbuf,
                i < m->ntex - 1 ? "," : "");
        }
        fprintf(f, "  }\n}\n");
    } else {
        int is_tool = !strcmp(type, "sword") || !strcmp(type, "pickaxe") || !strcmp(type, "axe")
            || !strcmp(type, "shovel") || !strcmp(type, "hoe");
        fprintf(f, "{\n  \"parent\": \"item/%s\",\n  \"textures\": {\n    \"layer0\": \"%s:item/%s\"\n  }\n}\n",
            is_tool ? "handheld" : "generated", mc_mod_id, name);
    }
    fclose(f);
}

static void cg_block_model_json(const char *name, int idx) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/assets/%s/models/block/%s.json",
        mc_mod_pkg, mc_mod_id, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    McModel *m = (idx >= 0 && idx < MAX_BLOCKS && mc_block_models[idx].parent[0])
        ? &mc_block_models[idx] : NULL;
    if (m) {
        fprintf(f, "{\n  \"parent\": \"block/%s\",\n  \"textures\": {\n", m->parent);
        for (int i = 0; i < m->ntex; i++) {
            const char *tv = m->tex_vals[i];
            int has_sep = strchr(tv, ':') || strchr(tv, '/');
            char tbuf[128];
            if (!has_sep) snprintf(tbuf, sizeof(tbuf), "%s:block/%s", mc_mod_id, tv);
            else snprintf(tbuf, sizeof(tbuf), "%s", tv);
            fprintf(f, "    \"%s\": \"%s\"%s\n", m->tex_keys[i], tbuf,
                i < m->ntex - 1 ? "," : "");
        }
        fprintf(f, "  }\n}\n");
    } else {
        fprintf(f, "{\n  \"parent\": \"block/cube_all\",\n  \"textures\": {\n    \"all\": \"%s:block/%s\"\n  }\n}\n",
            mc_mod_id, name);
    }
    fclose(f);
}

static void cg_blockstate_json(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/assets/%s/blockstates/%s.json",
        mc_mod_pkg, mc_mod_id, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n  \"variants\": {\n    \"\": {\n      \"model\": \"%s:block/%s\"\n    }\n  }\n}\n",
        mc_mod_id, name);
    fclose(f);
}

static void cg_lang_json(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/assets/%s/lang/en_us.json",
        mc_mod_pkg, mc_mod_id);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n");
    int first = 1;
    for (int i = 0; i < mc_item_count; i++) {
        if (!first) fprintf(f, ",\n");
        char dn[128];
        if (mc_items[i].display_name) {
            snprintf(dn, sizeof(dn), "%s", mc_items[i].display_name);
        } else {
            char *p = dn;
            for (char *c = mc_items[i].name; *c && p < dn + sizeof(dn) - 1; c++) {
                if (*c == '_') *p++ = ' ';
                else if (*c >= 'a' && *c <= 'z') *p++ = *c - 32;
                else *p++ = *c;
            }
            *p = 0;
        }
        fprintf(f, "  \"item.%s.%s\": \"%s\"", mc_mod_id, mc_items[i].name, dn);
        first = 0;
    }
    for (int i = 0; i < mc_block_count; i++) {
        if (!first) fprintf(f, ",\n");
        char dn[128];
        if (mc_blocks[i].display_name) {
            snprintf(dn, sizeof(dn), "%s", mc_blocks[i].display_name);
        } else {
            char *p = dn;
            for (char *c = mc_blocks[i].name; *c && p < dn + sizeof(dn) - 1; c++) {
                if (*c == '_') *p++ = ' ';
                else if (*c >= 'a' && *c <= 'z') *p++ = *c - 32;
                else *p++ = *c;
            }
            *p = 0;
        }
        fprintf(f, "  \"block.%s.%s\": \"%s\"", mc_mod_id, mc_blocks[i].name, dn);
        first = 0;
    }
    for (int i = 0; i < mc_tab_count; i++) {
        if (!first) fprintf(f, ",\n");
        char *dn = mc_tab_display_names[i] ? mc_tab_display_names[i] : mc_tabs[i].name;
        fprintf(f, "  \"creativetab.%s.%s\": \"%s\"", mc_mod_id, mc_tabs[i].name, dn);
        first = 0;
    }
    fprintf(f, "\n}\n");
    fclose(f);
}

static void cg_block_loot_json(const char *name, const char *drop) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/data/%s/loot_tables/blocks/%s.json",
        mc_mod_pkg, mc_mod_id, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    if (drop && strcmp(drop, name)) {
        fprintf(f, "{\n  \"type\": \"minecraft:block\",\n  \"pools\": [{\n");
        fprintf(f, "    \"bonus_rolls\": 0.0,\n    \"conditions\": [{\"condition\": \"minecraft:survives_explosion\"}],\n");
        fprintf(f, "    \"entries\": [{\"type\": \"minecraft:item\", \"name\": \"%s:%s\"}],\n", mc_mod_id, drop);
        fprintf(f, "    \"rolls\": 1.0\n  }]\n}\n");
    } else {
        fprintf(f, "{\n  \"type\": \"minecraft:block\",\n  \"pools\": [{\n");
        fprintf(f, "    \"bonus_rolls\": 0.0,\n    \"conditions\": [{\"condition\": \"minecraft:survives_explosion\"}],\n");
        fprintf(f, "    \"entries\": [{\"type\": \"minecraft:item\", \"name\": \"%s:%s\"}],\n", mc_mod_id, name);
        fprintf(f, "    \"rolls\": 1.0\n  }]\n}\n");
    }
    fclose(f);
}

static const char *mc_item_id(const char *val) {
    static char buf[128];
    if (strchr(val, ':')) return val;
    snprintf(buf, sizeof(buf), "%s:%s", mc_mod_id, val);
    return buf;
}

static void cg_recipe_json(McRecipe *r) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/data/%s/recipes/%s.json",
        mc_mod_pkg, mc_mod_id, r->name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    if (r->type == 0 && r->nrows > 0) {
        fprintf(f, "{\n  \"type\": \"minecraft:crafting_shaped\",\n  \"pattern\": [\n");
        for (int i = 0; i < r->nrows; i++) {
            fprintf(f, "    \"%s\"%s\n", r->pattern_rows[i], i < r->nrows - 1 ? "," : "");
        }
        fprintf(f, "  ],\n  \"key\": {\n");
        for (int i = 0; i < r->nkeys; i++) {
            fprintf(f, "    \"%c\": {\"item\": \"%s\"}%s\n", r->keys[i], mc_item_id(r->key_values[i]),
                i < r->nkeys - 1 ? "," : "");
        }
        fprintf(f, "  },\n  \"result\": {\"item\": \"%s\", \"count\": %d}\n}\n",
            mc_item_id(r->result), r->count);
    } else if (r->type == 1) {
        fprintf(f, "{\n  \"type\": \"minecraft:crafting_shapeless\",\n  \"ingredients\": [\n");
        for (int i = 0; i < r->ningredients; i++) {
            fprintf(f, "    {\"item\": \"%s\"}%s\n", mc_item_id(r->ingredients[i]),
                i < r->ningredients - 1 ? "," : "");
        }
        fprintf(f, "  ],\n  \"result\": {\"item\": \"%s\", \"count\": %d}\n}\n",
            mc_item_id(r->result), r->count);
    } else if (r->type == 2) {
        fprintf(f, "{\n  \"type\": \"minecraft:smelting\",\n  \"cookingtime\": %d,\n", r->cook_time);
        fprintf(f, "  \"experience\": %ff,\n  \"ingredient\": {\"item\": \"%s\"},\n",
            r->experience, mc_item_id(r->ingredient));
        fprintf(f, "  \"result\": \"%s\"\n}\n", mc_item_id(r->result));
    }
    fclose(f);
}

static void cg_block_tags(void) {
    static char tmp[32][MAX_BLOCKS][128];
    int ntmp[32] = {0};
    char tpath[32][512];
    int npaths = 0;

    for (int i = 0; i < mc_block_count; i++) {
        McBlock *bl = &mc_blocks[i];
        char *tool = "pickaxe", *tier = "";
        if (bl->prop_block && bl->prop_block->type == NODE_BLOCK) {
            for (int j = 0; j < bl->prop_block->data.block.count; j++) {
                ASTNode *s = bl->prop_block->data.block.stmts[j];
                if (s->type == NODE_ASSIGN) {
                    if (!strcmp(s->data.assign.name, "tool") && s->data.assign.value->type == NODE_STR)
                        tool = s->data.assign.value->data.str;
                    else if (!strcmp(s->data.assign.name, "tier") && s->data.assign.value->type == NODE_STR)
                        tier = s->data.assign.value->data.str;
                }
            }
        }
        if (tool) {
            char path[512];
            snprintf(path, sizeof(path), "%s/src/main/resources/data/minecraft/tags/blocks/mineable/%s.json",
                mc_mod_pkg, tool);
            int found = -1;
            for (int k = 0; k < npaths; k++)
                if (!strcmp(tpath[k], path)) { found = k; break; }
            if (found < 0) { found = npaths; if (found < 32) { npaths++; strcpy(tpath[found], path); ntmp[found] = 0; } }
            if (ntmp[found] < 64) strcpy(tmp[found][ntmp[found]++], bl->name);
        }
        if (tier && tier[0]) {
            char *tier_tag = !strcmp(tier, "iron") ? "needs_iron_tool"
                : !strcmp(tier, "stone") ? "needs_stone_tool"
                : !strcmp(tier, "diamond") ? "needs_diamond_tool" : NULL;
            if (tier_tag) {
                char path[512];
                snprintf(path, sizeof(path), "%s/src/main/resources/data/minecraft/tags/blocks/%s.json",
                    mc_mod_pkg, tier_tag);
                int found = -1;
                for (int k = 0; k < npaths; k++)
                    if (!strcmp(tpath[k], path)) { found = k; break; }
                if (found < 0) { found = npaths; if (found < 32) { npaths++; strcpy(tpath[found], path); ntmp[found] = 0; } }
                if (ntmp[found] < 64) strcpy(tmp[found][ntmp[found]++], bl->name);
            }
        }
    }

    for (int i = 0; i < npaths; i++) {
        FILE *f = fopen(tpath[i], "w");
        if (!f) continue;
        fprintf(f, "{\"replace\":false,\"values\":[");
        for (int j = 0; j < ntmp[i]; j++) {
            if (j > 0) fprintf(f, ",");
            fprintf(f, "\"%s:%s\"", mc_mod_id, tmp[i][j]);
        }
        fprintf(f, "]}\n");
        fclose(f);
    }
}

static void cg_mods_toml(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/META-INF/mods.toml", mc_mod_pkg);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "modLoader=\"javafml\"\nloaderVersion=\"[47,)\"\n");
    fprintf(f, "[[mods]]\nmodId=\"%s\"\nversion=\"%s\"\ndisplayName=\"%s\"\n",
        mc_mod_id,
        mc_prop_val("modVersion") ? mc_prop_val("modVersion") : "1.0.0",
        mc_prop_val("modName") ? mc_prop_val("modName") : mc_mod_id);
    if (mc_prop_val("modAuthors"))
        fprintf(f, "authors=\"%s\"\n", mc_prop_val("modAuthors"));
    fprintf(f, "license=\"%s\"\n", mc_prop_val("modLicense") ? mc_prop_val("modLicense") : "All Rights Reserved");
    fprintf(f, "[[dependencies.%s]]\n    modId=\"forge\"\n    mandatory=true\n    versionRange=\"[47,)\"\n    ordering=\"NONE\"\n    side=\"BOTH\"\n", mc_mod_id);
    fprintf(f, "[[dependencies.%s]]\n    modId=\"minecraft\"\n    mandatory=true\n    versionRange=\"[1.20.1,1.21)\"\n    ordering=\"NONE\"\n    side=\"BOTH\"\n", mc_mod_id);
    fclose(f);
}

static void cg_pack_mcmeta(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/src/main/resources/pack.mcmeta", mc_mod_pkg);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n  \"pack\": {\n    \"description\": \"%s resources\",\n    \"pack_format\": 15\n  }\n}\n", mc_mod_id);
    fclose(f);
}

static unsigned int crc32_tab[256];
static int crc32_ready;

static void crc32_make_tab(void) {
    for (unsigned int i = 0; i < 256; i++) {
        unsigned int crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        crc32_tab[i] = crc;
    }
    crc32_ready = 1;
}

static unsigned int crc32_buf(const unsigned char *buf, int len) {
    if (!crc32_ready) crc32_make_tab();
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
        crc = crc32_tab[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static unsigned int adler32_buf(const unsigned char *buf, int len) {
    unsigned int a = 1, b = 0;
    for (int i = 0; i < len; i++) {
        a = (a + buf[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

static void png_chunk(FILE *f, const char *type, const unsigned char *data, int len) {
    unsigned int l = len;
    unsigned char hdr[4];
    hdr[0] = (l >> 24) & 0xFF; hdr[1] = (l >> 16) & 0xFF;
    hdr[2] = (l >> 8) & 0xFF; hdr[3] = l & 0xFF;
    fwrite(hdr, 1, 4, f);
    fwrite(type, 1, 4, f);
    unsigned char *tmp = malloc(len + 4);
    memcpy(tmp, type, 4);
    if (len > 0) memcpy(tmp + 4, data, len);
    unsigned int crc = crc32_buf(tmp, len + 4);
    free(tmp);
    fwrite(data, 1, len, f);
    hdr[0] = (crc >> 24) & 0xFF; hdr[1] = (crc >> 16) & 0xFF;
    hdr[2] = (crc >> 8) & 0xFF; hdr[3] = crc & 0xFF;
    fwrite(hdr, 1, 4, f);
}

static unsigned int tex_name_hash(const char *name) {
    unsigned int h = 0;
    for (const char *p = name; *p; p++)
        h = h * 31 + (unsigned char)*p;
    return h;
}

static void cg_texture_png(const char *tex_path, const char *name) {
    FILE *f = fopen(tex_path, "wb");
    if (!f) return;
    unsigned char sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);
    unsigned char ihdr[13] = {0,0,0,16,0,0,0,16,8,6,0,0,0};
    png_chunk(f, "IHDR", ihdr, 13);
    unsigned int h = tex_name_hash(name);
    int r = (int)(((h & 0xFF) * 0.5) + 60);
    int g = (int)((((h >> 8) & 0xFF) * 0.5) + 60);
    int b = (int)((((h >> 16) & 0xFF) * 0.5) + 60);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    unsigned char raw[1040];
    for (int row = 0; row < 16; row++) {
        raw[row * 65] = 0;
        for (int col = 0; col < 16; col++) {
            int px = row * 65 + 1 + col * 4;
            raw[px] = r; raw[px+1] = g; raw[px+2] = b; raw[px+3] = 255;
        }
    }
    int rawlen = 1040;
    unsigned char deflate_hdr[5];
    deflate_hdr[0] = 0x01;
    deflate_hdr[1] = rawlen & 0xFF;
    deflate_hdr[2] = (rawlen >> 8) & 0xFF;
    deflate_hdr[3] = (~rawlen) & 0xFF;
    deflate_hdr[4] = (~(rawlen >> 8)) & 0xFF;
    unsigned char zlib_hdr[2];
    zlib_hdr[0] = 0x78; zlib_hdr[1] = 0x01;
    unsigned int adler = adler32_buf(raw, rawlen);
    unsigned char adler_buf[4];
    adler_buf[0] = (adler >> 24) & 0xFF; adler_buf[1] = (adler >> 16) & 0xFF;
    adler_buf[2] = (adler >> 8) & 0xFF; adler_buf[3] = adler & 0xFF;
    unsigned char *idat = malloc(2 + 5 + rawlen + 4);
    int pos = 0;
    memcpy(idat + pos, zlib_hdr, 2); pos += 2;
    memcpy(idat + pos, deflate_hdr, 5); pos += 5;
    memcpy(idat + pos, raw, rawlen); pos += rawlen;
    memcpy(idat + pos, adler_buf, 4); pos += 4;
    png_chunk(f, "IDAT", idat, pos);
    free(idat);
    png_chunk(f, "IEND", (unsigned char*)"", 0);
    fclose(f);
}

static void mkdir_p(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
    system(cmd);
}

static void cg_textures_all(void) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%ssrc/main/resources/assets/%s/textures/item", mc_mod_pkg, mc_mod_id);
    mkdir_p(dir);
    snprintf(dir, sizeof(dir), "%ssrc/main/resources/assets/%s/textures/block", mc_mod_pkg, mc_mod_id);
    mkdir_p(dir);
    for (int i = 0; i < mc_item_count; i++) {
        McModel *m = (i < MAX_ITEMS && mc_item_models[i].parent[0]) ? &mc_item_models[i] : NULL;
        if (m) {
            for (int j = 0; j < m->ntex; j++) {
                char tp[512];
                snprintf(tp, sizeof(tp), "%ssrc/main/resources/assets/%s/textures/item/%s.png",
                    mc_mod_pkg, mc_mod_id, m->tex_vals[j]);
                if (access(tp, F_OK) != 0)
                    cg_texture_png(tp, m->tex_vals[j]);
            }
        } else {
            char tp[512];
            snprintf(tp, sizeof(tp), "%ssrc/main/resources/assets/%s/textures/item/%s.png",
                mc_mod_pkg, mc_mod_id, mc_items[i].name);
            if (access(tp, F_OK) != 0)
                cg_texture_png(tp, mc_items[i].name);
        }
    }
    for (int i = 0; i < mc_block_count; i++) {
        McModel *m = (i < MAX_BLOCKS && mc_block_models[i].parent[0]) ? &mc_block_models[i] : NULL;
        if (m) {
            for (int j = 0; j < m->ntex; j++) {
                char tp[512];
                snprintf(tp, sizeof(tp), "%ssrc/main/resources/assets/%s/textures/block/%s.png",
                    mc_mod_pkg, mc_mod_id, m->tex_vals[j]);
                if (access(tp, F_OK) != 0)
                    cg_texture_png(tp, m->tex_vals[j]);
            }
        } else {
            char tp[512];
            snprintf(tp, sizeof(tp), "%ssrc/main/resources/assets/%s/textures/block/%s.png",
                mc_mod_pkg, mc_mod_id, mc_blocks[i].name);
            if (access(tp, F_OK) != 0)
                cg_texture_png(tp, mc_blocks[i].name);
        }
    }
}

static void cg_to_java_build_gradle(FILE *f) {
    if (!f) return;
    fprintf(f, "plugins {\n    id 'eclipse'\n    id 'idea'\n    id 'net.minecraftforge.gradle' version '[6.0,7.0)'\n");
    fprintf(f, "}\n\n");
    fprintf(f, "base {\n    archivesName = \"%s\"\n}\n", mc_mod_id);
    fprintf(f, "java.toolchain.languageVersion = JavaLanguageVersion.of(17)\n\n");
    fprintf(f, "minecraft {\n");
    fprintf(f, "    mappings channel: 'official', version: '%s'\n", mc_prop_val("mcVersion") ? mc_prop_val("mcVersion") : "1.20.1");
    fprintf(f, "    runs {\n");
    fprintf(f, "        client { workingDirectory file('run/client') }\n");
    fprintf(f, "        server { workingDirectory file('run/server') }\n");
    fprintf(f, "        data {\n            workingDirectory file('run/data')\n");
    fprintf(f, "            args '--mod', '%s', '--all', '--output', file('src/generated/resources/'), '--existing', file('src/main/resources/')\n", mc_mod_id);
    fprintf(f, "        }\n    }\n}\n\n");
    fprintf(f, "dependencies {\n    minecraft 'net.minecraftforge:forge:%s-%s'\n", mc_prop_val("mcVersion") ? mc_prop_val("mcVersion") : "1.20.1", mc_prop_val("forgeVersion") ? mc_prop_val("forgeVersion") : "47.3.0");
    fprintf(f, "}\n\n");
    fprintf(f, "jar {\n    from(\"LICENSE\") { rename { \"${it}_${base.archivesName.get()}\" } }\n}\n");
}

int generate_java(const char *path, const char *outpath) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return 1; }
    memset(lib_imported, 0, sizeof(lib_imported));
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *src = malloc(sz + 1);
    if (!src) { fclose(f); fprintf(stderr, "out of memory\n"); return 1; }
    long got = fread(src, 1, sz, f);
    fclose(f); f = NULL;
    if (got < sz) fprintf(stderr, "warning: short read from '%s'\n", path);
    src[got] = 0;

    char *psrc = src;
    while (*psrc == ' ' || *psrc == '\t' || *psrc == '\n' || *psrc == '\r') psrc++;
    if (psrc[0] == '[' && strncmp(psrc, "[MCMForge]", 10) == 0
        && (psrc[10] == '\n' || psrc[10] == '\r' || psrc[10] == 0)) {
        psrc = psrc + 10;
        while (*psrc == '\n' || *psrc == '\r') psrc++;
    }
    mc_output_filename[0] = 0;
    gradle_properties_mode = 0;
    while (*psrc == ' ' || *psrc == '\t' || *psrc == '\n' || *psrc == '\r') psrc++;
    if (psrc[0] == '(' && psrc[1] == '[') {
        char *cp = psrc + 2;
        while (*cp && *cp != ']' && *cp != '\n') cp++;
        if (*cp == ']' && cp > psrc + 2) {
            size_t flen = cp - (psrc + 2);
            snprintf(mc_output_filename, sizeof(mc_output_filename), "%.*s", (int)flen, psrc + 2);
            if (strstr(mc_output_filename, "gradle.properties"))
                gradle_properties_mode = 1;
        }
    }

    mc_prop_count = 0;
    mc_item_count = 0;
    mc_block_count = 0;
    mc_recipe_count = 0;
    mc_tab_count = 0;
    mc_name_seq_counter = 0;
    memset(mc_item_models, 0, sizeof(mc_item_models));
    memset(mc_block_models, 0, sizeof(mc_block_models));
    for (int i = 0; i < MAX_TABS; i++) {
        if (mc_tab_display_names[i]) { free(mc_tab_display_names[i]); mc_tab_display_names[i] = NULL; }
    }
    scan_mc_all(src);

    mc_name_consumer = 0;

    if (setjmp(error_jmp)) { free(src); return 1; }

    scope_depth = 0;
    anon_counter = 0;
    lex_init(psrc);
    lex_next();
    ASTNode *prog = parse_program();

    cg_to_registry_or_proc_file(prog);

    size_t olen = strlen(outpath);
    int dir_mode = (olen > 0 && outpath[olen - 1] == '/');

    if (access(outpath, F_OK) == 0) {
        if (dir_mode) {
            fprintf(stderr, "WARNING: target directory '%s' already has files. overwrite? [y/N]: ", outpath);
        } else {
            fprintf(stderr, "WARNING: '%s' already exists. overwrite? [y/N]: ", outpath);
        }
        char ans[16];
        if (!fgets(ans, sizeof(ans), stdin)) { free(src); return 1; }
        if (ans[0] != 'y' && ans[0] != 'Y') { free(src); return 0; }
    }

    if (dir_mode) {
        snprintf(mc_mod_pkg, sizeof(mc_mod_pkg), "%s", outpath);

        char d[1024];
        char java_src_dir[1024];

        if (mcm_flat_mode) {
            snprintf(java_src_dir, sizeof(java_src_dir), "%s", outpath);
        } else {
            snprintf(java_src_dir, sizeof(java_src_dir), "%ssrc/main/java/com/%s/%s/", outpath, mc_mod_id, mc_mod_id);
            mkdir_p(java_src_dir);
        }
        snprintf(d, sizeof(d), "%ssrc/main/resources/META-INF", outpath);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/assets/%s/models/item", outpath, mc_mod_id);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/assets/%s/models/block", outpath, mc_mod_id);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/assets/%s/blockstates", outpath, mc_mod_id);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/assets/%s/lang", outpath, mc_mod_id);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/data/%s/recipes", outpath, mc_mod_id);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/data/%s/loot_tables/blocks", outpath, mc_mod_id);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/data/minecraft/tags/blocks/mineable", outpath);
        mkdir_p(d);
        snprintf(d, sizeof(d), "%ssrc/main/resources/data/minecraft/tags/blocks", outpath);
        mkdir_p(d);

        snprintf(d, sizeof(d), "%sgradle.properties", outpath);
        f = fopen(d, "w");
        if (f) { cg_to_gradle_properties(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }

        snprintf(d, sizeof(d), "%sbuild.gradle", outpath);
        f = fopen(d, "w");
        if (f) { cg_to_java_build_gradle(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }

        snprintf(d, sizeof(d), "%sModMain.java", java_src_dir);
        f = fopen(d, "w");
        if (f) { cg_to_java_main(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }

        if (mc_item_count || mc_block_count) {
            snprintf(d, sizeof(d), "%sModItems.java", java_src_dir);
            f = fopen(d, "w");
            if (f) { cg_to_java_item_registry(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }
        }

        if (mc_block_count) {
            snprintf(d, sizeof(d), "%sModBlocks.java", java_src_dir);
            f = fopen(d, "w");
            if (f) { cg_to_java_block_registry(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }
        }

        if (mc_tab_count) {
            snprintf(d, sizeof(d), "%sModCreativeTabs.java", java_src_dir);
            f = fopen(d, "w");
            if (f) { cg_to_java_creative_tabs(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }
        }

        if (mc_armor_material_count) {
            snprintf(d, sizeof(d), "%sModArmorMaterials.java", java_src_dir);
            f = fopen(d, "w");
            if (f) { cg_to_java_armor_materials(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }
        }

        if (mc_tool_tier_count) {
            snprintf(d, sizeof(d), "%sModToolTiers.java", java_src_dir);
            f = fopen(d, "w");
            if (f) { cg_to_java_tool_tiers(f); fclose(f); fprintf(stdout, "generated: %s\n", d); }
        }

        for (int i = 0; i < mc_item_count; i++) {
            snprintf(d, sizeof(d), "%s%s.java", java_src_dir, mc_items[i].filename);
            f = fopen(d, "w");
            if (f) { cg_to_java_item_class(f, &mc_items[i]); fclose(f); fprintf(stdout, "generated: %s\n", d); }
        }

        for (int i = 0; i < mc_block_count; i++) {
            if (mc_blocks[i].proc_block) {
                snprintf(d, sizeof(d), "%s%s.java", java_src_dir, mc_blocks[i].filename);
                f = fopen(d, "w");
                if (f) { cg_to_java_block_class(f, &mc_blocks[i]); fclose(f); fprintf(stdout, "generated: %s\n", d); }
            }
        }

        for (int i = 0; i < mc_item_count; i++)
            cg_item_model_json(mc_items[i].name, mc_items[i].type, i);
        for (int i = 0; i < mc_block_count; i++) {
            cg_block_model_json(mc_blocks[i].name, i);
            cg_blockstate_json(mc_blocks[i].name);
            cg_block_loot_json(mc_blocks[i].name, mc_blocks[i].name);
        }
        cg_lang_json();
        for (int i = 0; i < mc_recipe_count; i++)
            cg_recipe_json(&mc_recipes[i]);
        cg_block_tags();
        cg_mods_toml();
        cg_pack_mcmeta();
        cg_textures_all();

        free(src);
        return 0;
    }


    if (mc_output_filename[0] && !gradle_properties_mode) {
        size_t olen2 = strlen(outpath);
        int out_is_dir = (olen2 > 0 && outpath[olen2 - 1] == '/');
        if (out_is_dir) {
            char *full = malloc(olen2 + strlen(mc_output_filename) + 1);
            memcpy(full, outpath, olen2);
            strcpy(full + olen2, mc_output_filename);
            outpath = full;
        } else {
            const char *last_slash = strrchr(path, '/');
            if (last_slash) {
                size_t dlen = last_slash - path + 1;
                char *full = malloc(dlen + strlen(mc_output_filename) + 1);
                memcpy(full, path, dlen);
                strcpy(full + dlen, mc_output_filename);
                outpath = full;
            } else {
                outpath = mc_output_filename;
            }
        }
    }

    if (gradle_properties_mode) {
        f = fopen(outpath, "w");
        if (!f) { fprintf(stderr, "error: cannot write '%s'\n", outpath); free(src); return 1; }
        cg_to_gradle_properties(f);
        fclose(f);
        printf("generated: %s\n", outpath);
        free(src);
        return 0;
    }

    int has_items = mc_item_count > 0;
    int is_item_file = 0, is_block_file = 0;
    if (mc_output_filename[0]) {
        for (int i = 0; i < mc_item_count; i++) {
            if (strstr(mc_output_filename, mc_items[i].filename)) {
                is_item_file = 1;
                break;
            }
        }
        if (!is_item_file) {
            for (int i = 0; i < mc_block_count; i++) {
                if (strstr(mc_output_filename, mc_blocks[i].filename)) {
                    is_block_file = 1;
                    break;
                }
            }
        }
    }

    if (is_item_file && mc_item_count > 0) {
        f = fopen(outpath, "w");
        if (!f) { fprintf(stderr, "error: cannot write '%s'\n", outpath); free(src); return 1; }
        for (int i = 0; i < mc_item_count; i++) {
            if (strstr(mc_output_filename, mc_items[i].filename)) {
                cg_to_java_item_class(f, &mc_items[i]);
                break;
            }
        }
        fclose(f);
        printf("generated: %s\n", outpath);
    } else if (is_block_file && mc_block_count > 0) {
        f = fopen(outpath, "w");
        if (!f) { fprintf(stderr, "error: cannot write '%s'\n", outpath); free(src); return 1; }
        for (int i = 0; i < mc_block_count; i++) {
            if (strstr(mc_output_filename, mc_blocks[i].filename)) {
                cg_to_java_block_class(f, &mc_blocks[i]);
                break;
            }
        }
        fclose(f);
        printf("generated: %s\n", outpath);
    } else if (has_items) {
        f = fopen(outpath, "w");
        if (!f) { fprintf(stderr, "error: cannot write '%s'\n", outpath); free(src); return 1; }
        cg_to_java_item_registry(f);
        fclose(f);
        printf("generated: %s\n", outpath);
    } else {
        f = fopen(outpath, "w");
        if (!f) { fprintf(stderr, "error: cannot write '%s'\n", outpath); free(src); return 1; }
        fprintf(f, "package com.%s.%s;\n\n", mc_mod_id, mc_mod_id);
        fprintf(f, "public class %s {\n", mc_output_filename[0] ? mc_output_filename : "Main");
        cg_java_block(f, prog, 1);
        fprintf(f, "}\n");
        fclose(f);
        printf("generated: %s\n", outpath);
    }

    free(src);
    return 0;
}