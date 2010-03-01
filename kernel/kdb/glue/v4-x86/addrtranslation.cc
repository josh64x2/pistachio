
#include <debug.h>
#include <kdb/kdb.h>
#include <kdb/cmd.h>
#include <kdb/input.h>

#include <linear_ptab.h>
#include INC_GLUE(hwspace.h)
#include INC_GLUE(space.h)
#include INC_ARCH(pgent.h)
#include INC_API(tcb.h)


DECLARE_CMD( cmd_virt_to_phys, root, 'i', "virt_to_phys", "Translate virtual address to physical address");

CMD( cmd_virt_to_phys, cg )
{
    threadid_t space_id;
    space_id.set_raw( get_hex("Space:", 0, NULL ) );
    addr_t vaddr = (addr_t)get_hex("Virtual Address", 0, NULL );
    cpuid_t cpu = (cpuid_t)get_dec("CPU", 0, NULL );
    
    space_t * space;
    if ( space_id.get_raw() == 0x0 )
         space = get_kernel_space();
    else
        space = get_kernel_space()->get_tcb(space_id)->get_space();
    
    pgent_t::pgsize_e size = pgent_t::size_max;
    word_t offset = 0;

#if defined(CONFIG_X_X86_HVM)
    if(space->is_hvm_space())
    {
	x86_hvm_space_t *hvm_space = space->get_hvm_space();
	tcb_t *tcb = hvm_space->get_tcb_list();
	addr_t gpaddr;
		
	tcb->get_arch()->dump_hvm_ptab_entry(vaddr);

	if(hvm_space->lookup_gphys_addr( vaddr, &gpaddr ))
	    vaddr = gpaddr;
    }
#endif    

    pgent_t * pgent = space->pgent(page_table_index(size, vaddr), cpu);
    printf( "PDIR @ %p\n", space->get_top_pdir_phys( cpu ) );
    
    if ( pgent->is_subtree( space, size ) )
    {
        size--;
        printf( "PTAB @ %p\n", pgent->pgent.get_ptab() );
        pgent = pgent->subtree( space, size );
        pgent = pgent->next( space, size, page_table_index(size, vaddr) );
        offset = (word_t)vaddr & ~X86_PAGE_MASK;
    }
    else
    {
        offset = (word_t)vaddr & ~X86_SUPERPAGE_MASK;
    }
        
    addr_t paddr = pgent->address( space, size );
    paddr = (addr_t)((word_t)paddr + offset);
    
    printf("[virt] %p -> [phys] %p ", vaddr, paddr );

    word_t pgsz = page_size (size);
    word_t rwx = pgent->reference_bits (space, size, vaddr);
    printf("%3d%cB %c%c%c (%c%c%c) %s ",
            (pgsz >= GB (1) ? pgsz >> 30 :
             pgsz >= MB (1) ? pgsz >> 20 : pgsz >> 10),
            pgsz >= GB (1) ? 'G' : pgsz >= MB (1) ? 'M' : 'K',
            pgent->is_readable (space, size)   ? 'r' : '~',
            pgent->is_writable (space, size)   ? 'w' : '~',
            pgent->is_executable (space, size) ? 'x' : '~',
            rwx & 4 ? 'R' : '~',
            rwx & 2 ? 'W' : '~',
            rwx & 1 ? 'X' : '~',
            pgent->is_kernel (space, size) ? "kernel" : "user");
    pgent->dump_misc (space, size);
    printf ("\n");
    
    return CMD_NOQUIT;
}

