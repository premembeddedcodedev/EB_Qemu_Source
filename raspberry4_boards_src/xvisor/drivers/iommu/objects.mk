#/**
# Copyright (c) 2016 Anup Patel.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of IOMMU drivers objects
# */

drivers-objs-$(CONFIG_IOMMU_IO_PGTABLE) += iommu/io-pgtable.o
drivers-objs-$(CONFIG_IOMMU_IO_PGTABLE_LPAE) += iommu/io-pgtable-arm.o
drivers-objs-$(CONFIG_IOMMU_IO_PGTABLE_ARMV7S) += iommu/io-pgtable-arm-v7s.o
drivers-objs-$(CONFIG_IPMMU_VMSA) += iommu/ipmmu-vmsa.o
drivers-objs-$(CONFIG_ARM_SMMU) += iommu/arm-smmu.o
