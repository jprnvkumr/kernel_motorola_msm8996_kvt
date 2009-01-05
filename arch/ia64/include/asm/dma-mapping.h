#ifndef _ASM_IA64_DMA_MAPPING_H
#define _ASM_IA64_DMA_MAPPING_H

/*
 * Copyright (C) 2003-2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <asm/machvec.h>
#include <linux/scatterlist.h>
#include <asm/swiotlb.h>

struct dma_mapping_ops {
	int             (*mapping_error)(struct device *dev,
					 dma_addr_t dma_addr);
	void*           (*alloc_coherent)(struct device *dev, size_t size,
				dma_addr_t *dma_handle, gfp_t gfp);
	void            (*free_coherent)(struct device *dev, size_t size,
				void *vaddr, dma_addr_t dma_handle);
	dma_addr_t      (*map_single)(struct device *hwdev, unsigned long ptr,
				size_t size, int direction);
	void            (*unmap_single)(struct device *dev, dma_addr_t addr,
				size_t size, int direction);
	dma_addr_t      (*map_single_attrs)(struct device *dev, void *cpu_addr,
					    size_t size, int direction,
					    struct dma_attrs *attrs);
	void		(*unmap_single_attrs)(struct device *dev,
					      dma_addr_t dma_addr,
					      size_t size, int direction,
					      struct dma_attrs *attrs);
	void            (*sync_single_for_cpu)(struct device *hwdev,
				dma_addr_t dma_handle, size_t size,
				int direction);
	void            (*sync_single_for_device)(struct device *hwdev,
				dma_addr_t dma_handle, size_t size,
				int direction);
	void            (*sync_single_range_for_cpu)(struct device *hwdev,
				dma_addr_t dma_handle, unsigned long offset,
				size_t size, int direction);
	void            (*sync_single_range_for_device)(struct device *hwdev,
				dma_addr_t dma_handle, unsigned long offset,
				size_t size, int direction);
	void            (*sync_sg_for_cpu)(struct device *hwdev,
				struct scatterlist *sg, int nelems,
				int direction);
	void            (*sync_sg_for_device)(struct device *hwdev,
				struct scatterlist *sg, int nelems,
				int direction);
	int             (*map_sg)(struct device *hwdev, struct scatterlist *sg,
				int nents, int direction);
	void            (*unmap_sg)(struct device *hwdev,
				struct scatterlist *sg, int nents,
				int direction);
	int             (*map_sg_attrs)(struct device *dev,
					struct scatterlist *sg, int nents,
					int direction, struct dma_attrs *attrs);
	void            (*unmap_sg_attrs)(struct device *dev,
					  struct scatterlist *sg, int nents,
					  int direction,
					  struct dma_attrs *attrs);
	int             (*dma_supported_op)(struct device *hwdev, u64 mask);
	int		is_phys;
};

extern struct dma_mapping_ops *dma_ops;
extern struct ia64_machine_vector ia64_mv;
extern void set_iommu_machvec(void);

#define dma_alloc_coherent(dev, size, handle, gfp)	\
	platform_dma_alloc_coherent(dev, size, handle, (gfp) | GFP_DMA)

/* coherent mem. is cheap */
static inline void *
dma_alloc_noncoherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
		      gfp_t flag)
{
	return dma_alloc_coherent(dev, size, dma_handle, flag);
}
#define dma_free_coherent	platform_dma_free_coherent
static inline void
dma_free_noncoherent(struct device *dev, size_t size, void *cpu_addr,
		     dma_addr_t dma_handle)
{
	dma_free_coherent(dev, size, cpu_addr, dma_handle);
}
#define dma_map_single_attrs	platform_dma_map_single_attrs
static inline dma_addr_t dma_map_single(struct device *dev, void *cpu_addr,
					size_t size, int dir)
{
	return dma_map_single_attrs(dev, cpu_addr, size, dir, NULL);
}
#define dma_map_sg_attrs	platform_dma_map_sg_attrs
static inline int dma_map_sg(struct device *dev, struct scatterlist *sgl,
			     int nents, int dir)
{
	return dma_map_sg_attrs(dev, sgl, nents, dir, NULL);
}
#define dma_unmap_single_attrs	platform_dma_unmap_single_attrs
static inline void dma_unmap_single(struct device *dev, dma_addr_t cpu_addr,
				    size_t size, int dir)
{
	return dma_unmap_single_attrs(dev, cpu_addr, size, dir, NULL);
}
#define dma_unmap_sg_attrs	platform_dma_unmap_sg_attrs
static inline void dma_unmap_sg(struct device *dev, struct scatterlist *sgl,
				int nents, int dir)
{
	return dma_unmap_sg_attrs(dev, sgl, nents, dir, NULL);
}
#define dma_sync_single_for_cpu	platform_dma_sync_single_for_cpu
#define dma_sync_sg_for_cpu	platform_dma_sync_sg_for_cpu
#define dma_sync_single_for_device platform_dma_sync_single_for_device
#define dma_sync_sg_for_device	platform_dma_sync_sg_for_device
#define dma_mapping_error	platform_dma_mapping_error

#define dma_map_page(dev, pg, off, size, dir)				\
	dma_map_single(dev, page_address(pg) + (off), (size), (dir))
#define dma_unmap_page(dev, dma_addr, size, dir)			\
	dma_unmap_single(dev, dma_addr, size, dir)

/*
 * Rest of this file is part of the "Advanced DMA API".  Use at your own risk.
 * See Documentation/DMA-API.txt for details.
 */

#define dma_sync_single_range_for_cpu(dev, dma_handle, offset, size, dir)	\
	dma_sync_single_for_cpu(dev, dma_handle, size, dir)
#define dma_sync_single_range_for_device(dev, dma_handle, offset, size, dir)	\
	dma_sync_single_for_device(dev, dma_handle, size, dir)

#define dma_supported		platform_dma_supported

static inline int
dma_set_mask (struct device *dev, u64 mask)
{
	if (!dev->dma_mask || !dma_supported(dev, mask))
		return -EIO;
	*dev->dma_mask = mask;
	return 0;
}

extern int dma_get_cache_alignment(void);

static inline void
dma_cache_sync (struct device *dev, void *vaddr, size_t size,
	enum dma_data_direction dir)
{
	/*
	 * IA-64 is cache-coherent, so this is mostly a no-op.  However, we do need to
	 * ensure that dma_cache_sync() enforces order, hence the mb().
	 */
	mb();
}

#define dma_is_consistent(d, h)	(1)	/* all we do is coherent memory... */

static inline struct dma_mapping_ops *get_dma_ops(struct device *dev)
{
	return dma_ops;
}



#endif /* _ASM_IA64_DMA_MAPPING_H */
