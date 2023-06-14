#ifndef BPT_H
#define BPT_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef UNIT_TEST
#include "predefined.h"
#else
// #include "unit_test_predefined.h"
#endif

namespace bpt {

/* offsets */
#define OFFSET_META 0
#define OFFSET_BLOCK OFFSET_META + sizeof(meta_t)
#define SIZE_NO_CHILDREN sizeof(leaf_node_t) - BP_ORDER * sizeof(record_t)

/* meta information of B+ tree */
// B+树信息
typedef struct {
    size_t order; /* `order` of B+ tree */
    size_t value_size; /* size of value */
    size_t key_size;   /* size of key */
    size_t internal_node_num; /* how many internal nodes 内部节点数*/
    size_t leaf_node_num;     /* how many leafs */
    size_t height;            /* height of tree (exclude leafs) */
    off_t slot;        /* where to store new block 存储块信息*/ 
    off_t root_offset; /* where is the root of internal nodes 内部节点的根的位置 */
    off_t leaf_offset; /* where is the first leaf */
} meta_t;

/* internal nodes' index segment */
//内部节点的索引段
struct index_t {
    key_t key;
    off_t child; /* child's offset */
};

/***
 * internal node block
 * 可以在其他地方以 internal_node_t::child_t 形式使用
 ***/
struct internal_node_t {
    typedef index_t * child_t;
    off_t parent; /* parent node offset */
    off_t next;
    off_t prev;
    size_t n; /* how many children 子节点个数*/
    index_t children[BP_ORDER]; 
};

/* the final record of value */
struct record_t {
    key_t key;
    value_t value;
};

/* leaf node block */
struct leaf_node_t {
    typedef record_t *child_t;

    off_t parent; /* parent node offset */
    off_t next;
    off_t prev;
    size_t n;
    record_t children[BP_ORDER];
};

/* the encapsulated B+ tree */
//封装B+树
class bplus_tree {
public:
    bplus_tree(const char *path, bool force_empty = false);

    /* abstract operations */
    int search(const key_t& key, value_t *value) const;

    int search_range(key_t *left, const key_t &right,
                     value_t *values, size_t max, bool *next = NULL) const;
    int remove(const key_t& key);
    int insert(const key_t& key, value_t value);
    int update(const key_t& key, value_t value);
    meta_t get_meta() const {
        return meta;
    };

#ifndef UNIT_TEST
private:
#else
public:
#endif
    char path[512];
    meta_t meta;//定义B+树信息

    /* init empty tree */
    void init_from_empty();//初始化为空

    /* find index */ 
    off_t search_index(const key_t &key) const;//根据索引查找

    /* find leaf */
    off_t search_leaf(off_t index, const key_t &key) const;
    off_t search_leaf(const key_t &key) const
    {
        return search_leaf(search_index(key), key);
    }

    /* remove internal node */
    void remove_from_index(off_t offset, internal_node_t &node,
                           const key_t &key);

    /* borrow one key from other internal node */
    bool borrow_key(bool from_right, internal_node_t &borrower,
                    off_t offset);

    /* borrow one record from other leaf */
    bool borrow_key(bool from_right, leaf_node_t &borrower);

    /* change one's parent key to another key */
    void change_parent_child(off_t parent, const key_t &o, const key_t &n);

    /* merge right leaf to left leaf */
    void merge_leafs(leaf_node_t *left, leaf_node_t *right);

    void merge_keys(index_t *where, internal_node_t &left,
                    internal_node_t &right);

    /* insert into leaf without split */
    void insert_record_no_split(leaf_node_t *leaf,
                                const key_t &key, const value_t &value);

    /* add key to the internal node */
    void insert_key_to_index(off_t offset, const key_t &key,
                             off_t value, off_t after);
    void insert_key_to_index_no_split(internal_node_t &node, const key_t &key,
                                      off_t value);

    /* change children's parent */
    void reset_index_children_parent(index_t *begin, index_t *end,
                                     off_t parent);

    template<class T>
    void node_create(off_t offset, T *node, T *next);

    template<class T>
    void node_remove(T *prev, T *node);

    /* multi-level file open/close 多级文件打开/关闭 */
    mutable FILE *fp;           //文件指针
    mutable int fp_level;       //跟踪文件的打开和关闭状态 0:关闭 1:打开
    //被mutable修饰的变量，将永远处于可变的状态，即使在一个const函数中
    void open_file(const char *mode = "rb+") const
    {
        // `rb+` 将确保我们可以在任何地方写入而不会截断文件
        if (fp_level == 0)
            fp = fopen(path, mode);

        ++fp_level;
    }

    void close_file() const
    {
        if (fp_level == 1)
            fclose(fp);

        --fp_level;
    }

    /* alloc from disk 从磁盘分配*/
    off_t alloc(size_t size)
    {
        off_t slot = meta.slot; //存储块信息
        meta.slot += size;//向后移动到下一个可以存储信息的位置
        return slot;//返回存储位置
    }

    off_t alloc(leaf_node_t *leaf)
    {
        leaf->n = 0;
        meta.leaf_node_num++;//B+数的叶子节点+1
        return alloc(sizeof(leaf_node_t));//返回存储叶子节点的位置
    }

    off_t alloc(internal_node_t *node)
    {
        node->n = 1; //子节点数
        meta.internal_node_num++; //内部节点数+1
        return alloc(sizeof(internal_node_t));//返回当前node存储的位置
    }

    void unalloc(leaf_node_t *leaf, off_t offset)
    {
        --meta.leaf_node_num;//叶子节点数-1
    }

    void unalloc(internal_node_t *node, off_t offset)
    {
        --meta.internal_node_num;//内部节点数-1
    }

    /* read block from disk 从磁盘读取块*/
    int map(void *block, off_t offset, size_t size) const
    {
        open_file();                            //打开存储文件
        fseek(fp, offset, SEEK_SET);            //SEEK_SET:基于文件开头偏移 SEEK_CUR:基于当前位置偏移 SEEK_END:基于文件末尾
        size_t rd = fread(block, size, 1, fp);  //写入数据到block
        close_file();

        return rd - 1;
    }

    template<class T>
    int map(T *block, off_t offset) const
    {
        return map(block, offset, sizeof(T));
    }

    /* write block to disk 将块写入磁盘*/
    int unmap(void *block, off_t offset, size_t size) const
    {
        open_file();
        fseek(fp, offset, SEEK_SET);
        size_t wd = fwrite(block, size, 1, fp);//写入数据
        close_file();

        return wd - 1;
    }
    template<class T>
    int unmap(T *block, off_t offset) const
    {
        return unmap(block, offset, sizeof(T));
    }
};

}

#endif /* end of BPT_H */
