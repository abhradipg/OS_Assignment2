#define _GNU_SOURCE
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/hugetlb_inline.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/namei.h>
#include <linux/types.h>

struct task_struct *task;
struct mm_struct *mm_curr;
struct vm_area_struct *vma;
const char *pathname = "/context";
char fileName[32] = "/context/mm_\0";
char pidString[8];
struct file *mmFile;
struct dentry *dentry;
struct path path1, path;
int perm = 0666;
int flags = O_RDWR | O_CREAT;
loff_t offset= 0, len = 1024 * 1024 * 1024;
pgd_t *pgd;
p4d_t *p4d;
pud_t *pud;
pmd_t *pmd;
pte_t *ptep, pte;
struct page *page;
struct inode *parent_inode;
struct savedPages *temp,*curr=NULL;


void itoa(int n, char *buff) {
    int i=0;
    do{
       buff[i]='0'+n%10;
       n/=10;
       i++;
    }while(n>0);
    buff[i]='\0';
}

void cleanContext(void){
    strcpy(fileName,"/context/mm_\0");
   	itoa((int)(current->pid), pidString);
	strcat(fileName, pidString);
	mmFile = current->mmFile;//filp_open(fileName, flags, mode);
	parent_inode = mmFile->f_path.dentry->d_parent->d_inode;
	inode_lock(parent_inode);
	vfs_unlink(current_user_ns(),parent_inode, mmFile->f_path.dentry, NULL);
	inode_unlock(parent_inode);
    curr=current->addressStart;
    while(curr!=NULL){
        temp=curr;
        curr=curr->next;
        kfree(temp);
    }
    //filp_close(mmFile, current->files);
	//printk("Process (pid: %d) Cleaning\n", current->pid);
}


SYSCALL_DEFINE1(mmcontext, int, mode)
{
    //int i=1,noOfPages=0;
    int dirNotExists;
    long address;
    curr=NULL;
    offset=0;
    strcpy(fileName,"/context/mm_\0");
    //printk("my_syscall_1 : %d\n", mode);
    //printk("%d\n",current->isContextSaved);
    if(mode==0){
       if(current->isContextSaved==1){
           return -EINVAL;
       }
       current->isContextSaved=1;
       
       //code to save
       mm_curr=current->mm;
       vma=mm_curr->mmap;
       itoa(current->pid, pidString);
       strcat(fileName, pidString);

       dirNotExists = kern_path(pathname, LOOKUP_FOLLOW, &path1);
       if (dirNotExists) {
	        dentry = kern_path_create(AT_FDCWD, pathname, &path, LOOKUP_DIRECTORY);
		    vfs_mkdir(current_user_ns(),path.dentry->d_inode, dentry, 0);
		    done_path_create(&path, dentry);
	    }
        if(current->fileCreated==0){
           mmFile = filp_open(fileName, flags, perm);
           current->mmFile=mmFile;
        }
        current->fileCreated=1;
        mmFile = current ->mmFile;
	    //vfs_fallocate(mmFile, 0, offset, len);
	    //filp_close(mmFile, current->files);

       while(vma!=NULL){
           //printk("mmap no. %d\n",i);
           //i++;
           if (!vma->vm_ops ) {
			    //printk("anonymous\n");
                if (vma->vm_start <= vma->vm_mm->start_stack && vma->vm_end >= vma->vm_mm->start_stack){
                     vma=vma->vm_next;
                     continue;
                    }
                //printk("vma start - %ld\n",vma->vm_start);
                //printk("vma_end - %ld\n",vma->vm_end);
               // printk("vma size--%ld\n",vma->vm_end-vma->vm_start);
               // printk("no of pages -- %ld\n",(vma->vm_end-vma->vm_start)/4096);
                if(is_vm_hugetlb_page(vma))
                   printk("is huge\n");
                address=vma->vm_start;
                while(address<vma->vm_end)
                {
                    pgd = pgd_offset(mm_curr, (unsigned long)address);
                    if (pgd_none(*pgd) || pgd_bad(*pgd))
                        goto next;

                    p4d = p4d_offset(pgd, (unsigned long)address);
                    if (p4d_none(*p4d) || p4d_bad(*p4d))
                        goto next;

                    pud = pud_offset(p4d, (unsigned long)address);
                    if(pud_none(*pud) || pud_bad(*pud))
                        goto next;

                    pmd = pmd_offset(pud, (unsigned long)address);
                    if (pmd_none(*pmd) || pmd_bad(*pmd))
                        goto next;

                    ptep = pte_offset_map(pmd, (unsigned long)address);
                    pte = *ptep;

                    if (!pte_present(pte))
                        goto next;
                    
                    temp=kmalloc(sizeof(*temp), GFP_KERNEL);
                    temp->address=(unsigned long)address;
                    temp->size=PAGE_SIZE;
                    temp->next=NULL;
                    if(curr==NULL){
                        current->addressStart=temp;
                        curr=temp;
                    }
                    else{
                        curr->next=temp;
                        curr=temp;
                    }
                    kernel_write(mmFile, (void *)address, PAGE_SIZE, &offset);
                    next:
                        address+=PAGE_SIZE;
                    }
		        }
           vma=vma->vm_next;
       }
       //filp_close(mmFile, current->files);
       //printk("total pages - %d\n",noOfPages);
       return 0;
    }
    else if(mode ==1){
       if(current->isContextSaved==0){
           return -EINVAL;
       }
       current->isContextSaved=0;
       //i=0;
       curr=current->addressStart;
       itoa(current->pid, pidString);
       strcat(fileName, pidString);
       mmFile = current->mmFile;//filp_open(fileName, flags, mode);
       while(curr!=NULL){
           //i++;
           kernel_read(mmFile, (void *)curr->address, curr->size, &offset);
           temp=curr;
           curr=curr->next;
           kfree(temp);
       }
       current->addressStart=NULL;
       //printk("pages restored- %d \n",i);
       //filp_close(mmFile, current->files);
       //curr=
       //cleanContext();
       return 0;
    }
    //printk("%d",EINVAL);
    return -EINVAL;
}