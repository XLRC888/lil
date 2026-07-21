# Minecraft Forge Mod Generator

lil can generate complete Minecraft Forge mods from a high-level DSL. Use `-j` to enable Java generation and `-o <dir>/` to write a full mod directory tree. Use `-jc` for flat output (files go directly in the target dir without the `src/main/java/...` tree).

## Quick Start

```lil
[MCMForge]
include minecraft-forge

modID@minecraft = "example_mod"
modVersion@minecraft = "0.1.0"
mcVersion@minecraft = "1.20.1"
modName@minecraft = "Example Mod"
modAuthors@minecraft = "you"
```

Compile: `lil -j mod.lil -o mod_out/`

For flat output (no `src/main/java/com/...` tree):

```bash
lil -jc mod.lil -o mod_out/
```

## Metadata

| Directive | Description |
|-----------|-------------|
| `modID@minecraft` | Mod identifier (used in file paths, registry names) |
| `modVersion@minecraft` | Mod version |
| `mcVersion@minecraft` | Minecraft version target |
| `forgeVersion@minecraft` | Forge version (default: 47.3.0) |
| `modName@minecraft` | Human-readable mod name |
| `modAuthors@minecraft` | Author string |
| `modLicense@minecraft` | Mod license (default: "All Rights Reserved") |

## Items

### Basic Items

```lil
newItem@minecraft(ruby) {
    propertiesItem@minecraft {
        stacksTo = 64
    }
    itemNames@minecraft {
        [1] = "Ruby"
    }
}
```

### Tools (automatic type detection)

Names containing `_sword`, `_pickaxe`, `_axe`, `_shovel`, or `_hoe` are automatically typed:

```lil
newItem@minecraft(ruby_sword, ruby_pickaxe, ruby_axe) {
    propertiesItem@minecraft {
        tier = "ruby"
        attackDamage = 3
        attackSpeed = -2.4
    }
    itemNames@minecraft {
        [1] = "Ruby Sword"
        [2] = "Ruby Pickaxe"
        [3] = "Ruby Axe"
    }
}
```

### Armor

Armor names (`_helmet`, `_chestplate`, `_leggings`, `_boots`) are detected:

```lil
newItem@minecraft(ruby_helmet, ruby_boots) {
    propertiesItem@minecraft {
        tier = "ruby"
        durability = 37
        protection = 3, 6, 8, 3
        toughness = 2.0
    }
    itemNames@minecraft {
        [1] = "Ruby Helmet"
        [2] = "Ruby Boots"
    }
}
```

### Food

```lil
newFood@minecraft(ruby_apple) {
    propertiesItem@minecraft {
        hunger = 6
        saturation = 0.6
        alwaysEdible = true
    }
    foodEffect@minecraft("NIGHT_VISION", 6000, 0, 1.0)
    foodEffect@minecraft("REGENERATION", 100, 1, 1.0)
}
```

### Properties

| Property | Applies To | Type | Description |
|----------|-----------|------|-------------|
| `tier` | tools, armor | string | Tool tier (e.g. "ruby", "diamond") |
| `attackDamage` | tools | number | Base attack damage |
| `attackSpeed` | tools | number | Attack speed modifier |
| `durability` | any | number | Item durability |
| `stacksTo` | items | number | Max stack size |
| `isImmuneToLava` | any | 0/1 | Fire resistance |
| `hunger` | food | number | Hunger restored |
| `saturation` | food | number | Saturation modifier |
| `alwaysEdible` | food | 0/1 | Edible when full |
| `protection` | armor | list | Per-slot protection values |
| `toughness` | armor | number | Armor toughness |

## Blocks

```lil
newBlock@minecraft(ruby_block, ruby_ore) {
    propertiesBlock@minecraft {
        material = "metal"
        hardness = 5.0
        resistance = 6.0
        tool = "pickaxe"
        tier = "iron"
        sound = "metal"
    }
    itemNames@minecraft {
        [1] = "Ruby Block"
        [2] = "Ruby Ore"
    }
}
```

### Block Properties

| Property | Values | Description |
|----------|--------|-------------|
| `material` | `metal`, `wood`, `stone`, `glass`, `dirt`, `plant`, `snow` | Base material |
| `hardness` | number | Mining hardness |
| `resistance` | number | Explosion resistance |
| `tool` | `pickaxe`, `axe`, `shovel`, `hoe` | Required tool type |
| `tier` | `stone`, `iron`, `diamond` | Required tool tier |
| `sound` | `metal`, `wood`, `stone`, `glass`, `deepslate`, `gravel`, `snow` | Block sound type |
| `requiresTool` | 0/1 | Tool required for drops |
| `light` | number (0-15) | Light emission level |

## Recipes

### Shaped

```lil
recipeShaped@minecraft(ruby_sword) {
    pattern {
        "", "R", "",
        "", "R", "",
        "", "S", ""
    }
    keys = "R": "ruby", "S": "minecraft:stick"
    result = "ruby_sword"
}
```

Pattern cells auto-group: 9 cells = 3x3 grid, 4 cells = 2x2 grid, anything else = 1 row per cell (old format `" R "` also works). Empty string `""` = empty slot in the grid.

Item references in keys (`"R": "ruby"`), results, and ingredients default to the current mod's namespace: `"ruby"` becomes `"lilmod:ruby"`. Prepend a modid with `:` to reference items from another mod or vanilla: `"minecraft:stick"`, `"othermod:ruby"`. This applies everywhere item IDs are used in recipes.

For comparison, the old format also works (same result):

```lil
recipeShaped@minecraft(ruby_sword) {
    pattern {
        " R ",
        " R ",
        " S "
    }
    keys = "R": "ruby", "S": "minecraft:stick"
    result = "ruby_sword"
}
```

### Shapeless

```lil
recipeShapeless@minecraft(ruby) {
    ingredients = "ruby_block"
    result = "ruby"
    count = 9
}
```

### Smelting

```lil
recipeSmelting@minecraft(ruby) {
    ingredient = "raw_ruby"
    result = "ruby"
    experience = 1.0
    cookTime = 200
}
```

## Creative Tabs

```lil
newCreativeTab@minecraft(ruby_tab) {
    propertiesCreativeTab@minecraft {
        icon = "ruby_sword"
        items = "ruby", "ruby_sword", "ruby_block", "ruby_apple"
    }
}
```

### Properties

| Property | Description |
|----------|-------------|
| `icon` | Item to use as tab icon |
| `items` | Comma-separated list of items to display in the tab |

### Display Names

Use `creativeTabNames@minecraft` inside the tab block to set per-tab display names (indexed by parameter position):

```lil
newCreativeTab@minecraft(materials_tab, combat_tab) {
    propertiesCreativeTab@minecraft {
        icon = "ruby"
        items = "ruby", "ruby_block", "ruby_sword", "ruby_helmet"
    }
    creativeTabNames@minecraft {
        [1] = "Materials"
        [2] = "Combat"
    }
}
```

If no display name is set, the tab's internal name is used as-is in the lang file.

## Procedure Events

Items and blocks can have custom behavior via procedure blocks:

### Item Events

```lil
procedureItem@minecraft(HitEntity) {
    write@std "hit something"
}
```

| Event Name | Override Method |
|------------|----------------|
| `RightClick` | `InteractionResultHolder<ItemStack> use(Level, Player, InteractionHand)` |
| `HitEntity` | `boolean hurtEnemy(ItemStack, LivingEntity, LivingEntity)` |
| `Crafted` | `void onCraftedBy(ItemStack, Level, Player)` |

### Block Events

```lil
procedureBlock@minecraft(BlockPlaced) {
    write@std "placed"
}
```

| Event Name | Override Method |
|------------|----------------|
| `BlockPlaced` | `void onPlace(BlockState, Level, BlockPos, BlockState, boolean)` |

## Custom Models

Override auto-generated item and block model JSONs with `modelsItem@minecraft` and `modelsBlock@minecraft`.

### Item Models

```lil
modelsItem@minecraft {
    [1] parent = "handheld"
    [1] layer0 = "ruby_sword"
}
```

Each `[N]` entry targets item index N (1-based, same order as in `newItem@minecraft`). `parent` sets the model parent; any other key becomes a texture variable in the generated JSON. Texture paths without a colon or slash get prefixed with `{modid}:item/`.

In the JSON output, this produces a [custom model](https://minecraft.wiki/w/Model) instead of the generic `item/generated` or `item/handheld`:

```json
{
  "parent": "item/handheld",
  "textures": {
    "layer0": "example_mod:item/ruby_sword"
  }
}
```

### Block Models

```lil
modelsBlock@minecraft {
    [1] parent = "cube_column"
    [1] end = "example_block_top"
    [1] side = "example_block_side"
}
```

Same format as item models. Texture paths without a colon or slash get prefixed with `{modid}:block/` instead of `item/`. Omitting both `modelsItem` and `modelsBlock` produces standard auto-generated models (`item/generated`, `block/cube_all`).

### Auto-Generated Textures

If you define a custom model referencing a texture file (e.g. `layer0 = "ruby_sword"`), and no `.png` file exists at the expected path, lil auto-generates a 16x16 placeholder PNG with a color derived from the texture name. This lets you test the mod in-game before creating real textures.

The expected paths are:
- Item textures: `src/main/resources/assets/{modid}/textures/item/{name}.png`
- Block textures: `src/main/resources/assets/{modid}/textures/block/{name}.png`

## Generated Files

When using `-o <dir>/`, lil produces a full mod source tree:

```
<dir>/
  build.gradle
  gradle.properties
  src/main/java/com/{modid}/
    ModMain.java
    ModItems.java
    ...
  src/main/resources/
    ...
```

With `-jc` (flat mode), Java files go directly in `<dir>/` without the `src/main/java/com/{modid}/` nesting. Resources and build config keep their same paths:

```
<dir>/
  build.gradle
  gradle.properties
  ModMain.java
  ModItems.java
  ModBlocks.java
  ModCreativeTabs.java
  ModArmorMaterials.java
  ModToolTiers.java
  {ClassName}.java           (one per item/block)
  src/main/resources/
    META-INF/mods.toml
    pack.mcmeta
    assets/{modid}/
      lang/en_us.json
      models/item/{name}.json
      models/block/{name}.json
      blockstates/{name}.json
    data/{modid}/
      recipes/{name}.json
      loot_tables/blocks/{name}.json
      tags/blocks/mineable/{tool}.json
      tags/blocks/needs_{tier}_tool.json
```
