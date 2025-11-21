/*
 * btree_search_demo.c
 * 
 * PostgreSQL B+树搜索算法演示程序
 * 
 * 本程序演示了B+树搜索的核心算法，包括：
 * 1. 树的下降过程
 * 2. 页面内二分查找
 * 3. 并发分裂处理（right-link跟随）
 * 4. 父页面栈的构建
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* ==================== 数据结构定义 ==================== */

#define MAX_KEYS_PER_PAGE 7      // 每页最大键数
#define MAX_CHILDREN 8           // 最大子节点数
#define INVALID_BLOCK 0xFFFFFFFF // 无效块号

// 块号类型
typedef unsigned int BlockNumber;

// 偏移号类型
typedef unsigned short OffsetNumber;

// 页面类型
typedef enum {
    PAGE_INTERNAL,  // 内部页面
    PAGE_LEAF       // 叶子页面
} PageType;

// 访问模式
typedef enum {
    BT_READ,   // 读模式
    BT_WRITE   // 写模式
} AccessMode;

// B树页面结构
typedef struct BTPage {
    PageType type;                          // 页面类型
    BlockNumber blockno;                    // 页面块号
    BlockNumber right_link;                 // 右兄弟指针
    int num_keys;                           // 键的数量
    int keys[MAX_KEYS_PER_PAGE];           // 键数组
    BlockNumber children[MAX_CHILDREN];     // 子页面指针（仅内部页面）
    int high_key;                           // high key（页面键范围上界）
    bool has_high_key;                      // 是否有high key
} BTPage;

// 父页面栈节点
typedef struct BTStackData {
    BlockNumber bts_blkno;                  // 父页面块号
    OffsetNumber bts_offset;                // 父页面中的偏移
    struct BTStackData *bts_parent;         // 指向更上层父页面
} BTStackData;

typedef BTStackData *BTStack;

// 扫描键结构（简化版）
typedef struct BTScanInsert {
    int scankey;        // 搜索键值
    bool nextkey;       // false: >=; true: >
} BTScanInsert;

// B树结构
typedef struct BTree {
    BTPage **pages;     // 页面数组
    int num_pages;      // 页面总数
    BlockNumber root;   // 根页面块号
} BTree;

/* ==================== 辅助函数 ==================== */

// 创建新页面
BTPage* create_page(PageType type, BlockNumber blockno) {
    BTPage *page = (BTPage*)malloc(sizeof(BTPage));
    page->type = type;
    page->blockno = blockno;
    page->right_link = INVALID_BLOCK;
    page->num_keys = 0;
    page->has_high_key = false;
    memset(page->keys, 0, sizeof(page->keys));
    memset(page->children, 0xFF, sizeof(page->children));
    return page;
}

// 获取页面
BTPage* get_page(BTree *tree, BlockNumber blockno) {
    if (blockno >= tree->num_pages) {
        return NULL;
    }
    return tree->pages[blockno];
}

// 检查是否为叶子页面
bool is_leaf(BTPage *page) {
    return page->type == PAGE_LEAF;
}

// 检查是否为最右页面
bool is_rightmost(BTPage *page) {
    return page->right_link == INVALID_BLOCK;
}

// 创建栈节点
BTStack create_stack_node(BlockNumber blkno, OffsetNumber offset, BTStack parent) {
    BTStack stack = (BTStack)malloc(sizeof(BTStackData));
    stack->bts_blkno = blkno;
    stack->bts_offset = offset;
    stack->bts_parent = parent;
    return stack;
}

// 释放栈
void free_stack(BTStack stack) {
    while (stack != NULL) {
        BTStack next = stack->bts_parent;
        free(stack);
        stack = next;
    }
}

// 打印栈
void print_stack(BTStack stack) {
    printf("Parent Stack (from leaf to root):\n");
    int level = 0;
    while (stack != NULL) {
        printf("  Level %d: Block=%u, Offset=%u\n", 
               level++, stack->bts_blkno, stack->bts_offset);
        stack = stack->bts_parent;
    }
}

/* ==================== 核心搜索算法 ==================== */

/*
 * _bt_compare - 比较扫描键和页面中指定位置的键
 * 
 * 返回值：
 *   < 0: scankey < page_key
 *   = 0: scankey == page_key
 *   > 0: scankey > page_key
 */
int _bt_compare(BTScanInsert *key, BTPage *page, OffsetNumber offnum) {
    // 内部页面的第一个键被视为"负无穷"
    if (!is_leaf(page) && offnum == 0) {
        return 1;  // scankey > 负无穷
    }
    
    if (offnum >= page->num_keys) {
        return -1;  // 超出范围
    }
    
    int page_key = page->keys[offnum];
    return key->scankey - page_key;
}

/*
 * _bt_binsrch - 页面内二分查找
 * 
 * 叶子页面：返回第一个 >= scankey (或 > scankey if nextkey=true) 的位置
 * 内部页面：返回最后一个 < scankey (或 <= scankey if nextkey=true) 的位置
 */
OffsetNumber _bt_binsrch(BTScanInsert *key, BTPage *page) {
    OffsetNumber low = 0;
    OffsetNumber high = page->num_keys;
    int cmpval = key->nextkey ? 0 : 1;
    
    printf("    Binary search on page %u (type=%s, num_keys=%d):\n",
           page->blockno, is_leaf(page) ? "LEAF" : "INTERNAL", page->num_keys);
    
    // 空页面处理
    if (high < low) {
        printf("      Empty page, return offset 0\n");
        return low;
    }
    
    // 二分查找
    high++;  // 建立循环不变式
    
    while (high > low) {
        OffsetNumber mid = low + ((high - low) / 2);
        int result = _bt_compare(key, page, mid);
        
        printf("      [low=%u, mid=%u, high=%u] compare(key=%d, page[%u]=%d) = %d\n",
               low, mid, high, key->scankey, mid, 
               mid < page->num_keys ? page->keys[mid] : -1, result);
        
        if (result >= cmpval) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    
    // 叶子页面：返回第一个>=或>的位置
    if (is_leaf(page)) {
        printf("      Leaf page: return offset %u\n", low);
        return low;
    }
    
    // 内部页面：返回最后一个<或<=的位置
    OffsetNumber result = (low > 0) ? (low - 1) : 0;
    printf("      Internal page: return offset %u (child block=%u)\n", 
           result, page->children[result]);
    return result;
}

/*
 * _bt_moveright - 向右移动处理并发分裂
 * 
 * 检查页面的high key，如果scankey超出范围，跟随right-link向右移动
 */
BTPage* _bt_moveright(BTree *tree, BTScanInsert *key, BTPage *page) {
    int cmpval = key->nextkey ? 0 : 1;
    int move_count = 0;
    
    while (true) {
        // 如果是最右页面，无需移动
        if (is_rightmost(page)) {
            if (move_count > 0) {
                printf("    Reached rightmost page %u after %d moves\n", 
                       page->blockno, move_count);
            }
            break;
        }
        
        // 检查high key
        if (page->has_high_key) {
            int cmp_result = key->scankey - page->high_key;
            
            printf("    Check high key: scankey=%d %s high_key=%d on page %u\n",
                   key->scankey, 
                   key->nextkey ? ">=" : ">",
                   page->high_key, page->blockno);
            
            // 判断是否需要向右移动
            if (cmp_result >= cmpval) {
                // 需要向右移动
                BlockNumber next_block = page->right_link;
                printf("    Moving right: %u -> %u\n", page->blockno, next_block);
                page = get_page(tree, next_block);
                move_count++;
                continue;
            }
        }
        
        // 找到正确的页面
        break;
    }
    
    return page;
}

/*
 * _bt_search - B树搜索主函数
 * 
 * 从根页面开始，下降到包含搜索键的叶子页面
 * 返回父页面栈，用于后续的插入操作
 */
BTStack _bt_search(BTree *tree, BTScanInsert *key, BTPage **leaf_page) {
    BTStack stack = NULL;
    BlockNumber current_block = tree->root;
    BTPage *page = get_page(tree, current_block);
    
    printf("\n=== Starting B-tree search for key=%d (nextkey=%s) ===\n",
           key->scankey, key->nextkey ? "true" : "false");
    
    // 循环下降树的每一层
    int level = 0;
    while (true) {
        printf("\n  Level %d: Visiting page %u\n", level, page->blockno);
        
        // 处理并发分裂（向右移动）
        page = _bt_moveright(tree, key, page);
        
        // 检查是否到达叶子页
        if (is_leaf(page)) {
            printf("  Reached leaf page %u\n", page->blockno);
            break;
        }
        
        // 在内部页面上二分查找
        OffsetNumber offnum = _bt_binsrch(key, page);
        
        // 获取子页面块号
        BlockNumber child_block = page->children[offnum];
        BlockNumber parent_block = page->blockno;
        
        printf("    Descending to child: page[%u].children[%u] = block %u\n",
               parent_block, offnum, child_block);
        
        // 保存父页面位置到栈
        BTStack new_stack = create_stack_node(parent_block, offnum, stack);
        stack = new_stack;
        
        // 移动到子页面
        page = get_page(tree, child_block);
        level++;
    }
    
    // 在叶子页面上执行最终的二分查找
    printf("\n  Final binary search on leaf page:\n");
    OffsetNumber leaf_offset = _bt_binsrch(key, page);
    
    printf("\n=== Search complete: found position %u on leaf page %u ===\n",
           leaf_offset, page->blockno);
    
    *leaf_page = page;
    return stack;
}

/* ==================== 测试用例 ==================== */

// 创建示例B树
BTree* create_sample_tree() {
    BTree *tree = (BTree*)malloc(sizeof(BTree));
    tree->num_pages = 11;
    tree->pages = (BTPage**)malloc(sizeof(BTPage*) * tree->num_pages);
    tree->root = 0;
    
    // 创建根页面（内部页面）
    // 键: [50, 100]
    // 子节点: [1, 2, 3]
    BTPage *root = create_page(PAGE_INTERNAL, 0);
    root->num_keys = 2;
    root->keys[0] = 50;
    root->keys[1] = 100;
    root->children[0] = 1;
    root->children[1] = 2;
    root->children[2] = 3;
    tree->pages[0] = root;
    
    // 第二层 - 左子树（内部页面）
    // 键: [20, 35]
    // 子节点: [4, 5, 6]
    BTPage *page1 = create_page(PAGE_INTERNAL, 1);
    page1->num_keys = 2;
    page1->keys[0] = 20;
    page1->keys[1] = 35;
    page1->children[0] = 4;
    page1->children[1] = 5;
    page1->children[2] = 6;
    page1->high_key = 50;
    page1->has_high_key = true;
    page1->right_link = 2;
    tree->pages[1] = page1;
    
    // 第二层 - 中子树（内部页面）
    // 键: [70, 85]
    // 子节点: [7, 8, 9]
    BTPage *page2 = create_page(PAGE_INTERNAL, 2);
    page2->num_keys = 2;
    page2->keys[0] = 70;
    page2->keys[1] = 85;
    page2->children[0] = 7;
    page2->children[1] = 8;
    page2->children[2] = 9;
    page2->high_key = 100;
    page2->has_high_key = true;
    page2->right_link = 3;
    tree->pages[2] = page2;
    
    // 第二层 - 右子树（内部页面）
    // 键: [120]
    // 子节点: [10, 11] (注意：11号页面不存在，仅作演示)
    BTPage *page3 = create_page(PAGE_INTERNAL, 3);
    page3->num_keys = 1;
    page3->keys[0] = 120;
    page3->children[0] = 10;
    page3->children[1] = 10;  // 简化，实际应该是另一个页面
    tree->pages[3] = page3;
    
    // 叶子页面 - 块4: [5, 10, 15]
    BTPage *leaf4 = create_page(PAGE_LEAF, 4);
    leaf4->num_keys = 3;
    leaf4->keys[0] = 5;
    leaf4->keys[1] = 10;
    leaf4->keys[2] = 15;
    leaf4->high_key = 20;
    leaf4->has_high_key = true;
    leaf4->right_link = 5;
    tree->pages[4] = leaf4;
    
    // 叶子页面 - 块5: [20, 25, 30]
    BTPage *leaf5 = create_page(PAGE_LEAF, 5);
    leaf5->num_keys = 3;
    leaf5->keys[0] = 20;
    leaf5->keys[1] = 25;
    leaf5->keys[2] = 30;
    leaf5->high_key = 35;
    leaf5->has_high_key = true;
    leaf5->right_link = 6;
    tree->pages[5] = leaf5;
    
    // 叶子页面 - 块6: [35, 40, 45]
    BTPage *leaf6 = create_page(PAGE_LEAF, 6);
    leaf6->num_keys = 3;
    leaf6->keys[0] = 35;
    leaf6->keys[1] = 40;
    leaf6->keys[2] = 45;
    leaf6->high_key = 50;
    leaf6->has_high_key = true;
    leaf6->right_link = 7;
    tree->pages[6] = leaf6;
    
    // 叶子页面 - 块7: [50, 55, 60, 65]
    BTPage *leaf7 = create_page(PAGE_LEAF, 7);
    leaf7->num_keys = 4;
    leaf7->keys[0] = 50;
    leaf7->keys[1] = 55;
    leaf7->keys[2] = 60;
    leaf7->keys[3] = 65;
    leaf7->high_key = 70;
    leaf7->has_high_key = true;
    leaf7->right_link = 8;
    tree->pages[7] = leaf7;
    
    // 叶子页面 - 块8: [70, 75, 80]
    BTPage *leaf8 = create_page(PAGE_LEAF, 8);
    leaf8->num_keys = 3;
    leaf8->keys[0] = 70;
    leaf8->keys[1] = 75;
    leaf8->keys[2] = 80;
    leaf8->high_key = 85;
    leaf8->has_high_key = true;
    leaf8->right_link = 9;
    tree->pages[8] = leaf8;
    
    // 叶子页面 - 块9: [85, 90, 95]
    BTPage *leaf9 = create_page(PAGE_LEAF, 9);
    leaf9->num_keys = 3;
    leaf9->keys[0] = 85;
    leaf9->keys[1] = 90;
    leaf9->keys[2] = 95;
    leaf9->high_key = 100;
    leaf9->has_high_key = true;
    leaf9->right_link = 10;
    tree->pages[9] = leaf9;
    
    // 叶子页面 - 块10: [100, 110, 115]
    BTPage *leaf10 = create_page(PAGE_LEAF, 10);
    leaf10->num_keys = 3;
    leaf10->keys[0] = 100;
    leaf10->keys[1] = 110;
    leaf10->keys[2] = 115;
    tree->pages[10] = leaf10;
    
    return tree;
}

// 打印树结构
void print_tree_structure(BTree *tree) {
    printf("\n=== B-tree Structure ===\n");
    printf("Root: Block %u\n", tree->root);
    printf("Total pages: %d\n\n", tree->num_pages);
    
    for (int i = 0; i < tree->num_pages; i++) {
        BTPage *page = tree->pages[i];
        printf("Block %u (%s):\n", page->blockno, 
               is_leaf(page) ? "LEAF" : "INTERNAL");
        printf("  Keys: [");
        for (int j = 0; j < page->num_keys; j++) {
            printf("%d%s", page->keys[j], j < page->num_keys - 1 ? ", " : "");
        }
        printf("]\n");
        
        if (!is_leaf(page)) {
            printf("  Children: [");
            for (int j = 0; j <= page->num_keys; j++) {
                printf("%u%s", page->children[j], j < page->num_keys ? ", " : "");
            }
            printf("]\n");
        }
        
        if (page->has_high_key) {
            printf("  High key: %d\n", page->high_key);
        }
        
        if (!is_rightmost(page)) {
            printf("  Right link: %u\n", page->right_link);
        }
        printf("\n");
    }
}

// 执行搜索测试
void test_search(BTree *tree, int search_key, bool nextkey) {
    BTScanInsert key;
    key.scankey = search_key;
    key.nextkey = nextkey;
    
    BTPage *leaf_page = NULL;
    BTStack stack = _bt_search(tree, &key, &leaf_page);
    
    printf("\n");
    print_stack(stack);
    
    printf("\nLeaf page content:\n");
    printf("  Block %u, Keys: [", leaf_page->blockno);
    for (int i = 0; i < leaf_page->num_keys; i++) {
        printf("%d%s", leaf_page->keys[i], i < leaf_page->num_keys - 1 ? ", " : "");
    }
    printf("]\n");
    
    free_stack(stack);
    printf("\n============================================================\n");
}

// 释放树
void free_tree(BTree *tree) {
    for (int i = 0; i < tree->num_pages; i++) {
        free(tree->pages[i]);
    }
    free(tree->pages);
    free(tree);
}

/* ==================== 主函数 ==================== */

int main() {
    printf("PostgreSQL B+Tree Search Algorithm Demo\n");
    printf("========================================\n");
    
    // 创建示例B树
    BTree *tree = create_sample_tree();
    
    // 打印树结构
    print_tree_structure(tree);
    
    // 测试用例1：查找存在的键
    printf("\n\n### Test 1: Search for existing key 75 (nextkey=false) ###\n");
    test_search(tree, 75, false);
    
    // 测试用例2：查找不存在的键
    printf("\n\n### Test 2: Search for non-existing key 72 (nextkey=false) ###\n");
    test_search(tree, 72, false);
    
    // 测试用例3：使用nextkey=true
    printf("\n\n### Test 3: Search for key 70 with nextkey=true ###\n");
    test_search(tree, 70, true);
    
    // 测试用例4：查找最小键
    printf("\n\n### Test 4: Search for minimum key 5 (nextkey=false) ###\n");
    test_search(tree, 5, false);
    
    // 测试用例5：查找最大键
    printf("\n\n### Test 5: Search for maximum key 115 (nextkey=false) ###\n");
    test_search(tree, 115, false);
    
    // 释放资源
    free_tree(tree);
    
    printf("\nDemo completed successfully!\n");
    return 0;
}
