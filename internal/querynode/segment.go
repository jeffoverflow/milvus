package querynode

/*

#cgo CFLAGS: -I${SRCDIR}/../core/output/include

#cgo LDFLAGS: -L${SRCDIR}/../core/output/lib -lmilvus_segcore -Wl,-rpath=${SRCDIR}/../core/output/lib

#include "segcore/collection_c.h"
#include "segcore/plan_c.h"
#include "segcore/reduce_c.h"
*/
import "C"
import (
	"strconv"
	"unsafe"

	"github.com/stretchr/testify/assert"

	"github.com/zilliztech/milvus-distributed/internal/errors"
	"github.com/zilliztech/milvus-distributed/internal/proto/commonpb"
)

type Segment struct {
	segmentPtr       C.CSegmentBase
	segmentID        UniqueID
	partitionTag     string // TODO: use partitionID
	collectionID     UniqueID
	lastMemSize      int64
	lastRowCount     int64
	recentlyModified bool
}

func (s *Segment) ID() UniqueID {
	return s.segmentID
}

//-------------------------------------------------------------------------------------- constructor and destructor
func newSegment(collection *Collection, segmentID int64, partitionTag string, collectionID UniqueID) *Segment {
	/*
		CSegmentBase
		newSegment(CPartition partition, unsigned long segment_id);
	*/
	segmentPtr := C.NewSegment(collection.collectionPtr, C.ulong(segmentID))
	var newSegment = &Segment{
		segmentPtr:   segmentPtr,
		segmentID:    segmentID,
		partitionTag: partitionTag,
		collectionID: collectionID,
	}

	return newSegment
}

func deleteSegment(segment *Segment) {
	/*
		void
		deleteSegment(CSegmentBase segment);
	*/
	cPtr := segment.segmentPtr
	C.DeleteSegment(cPtr)
}

//-------------------------------------------------------------------------------------- stats functions
func (s *Segment) getRowCount() int64 {
	/*
		long int
		getRowCount(CSegmentBase c_segment);
	*/
	var rowCount = C.GetRowCount(s.segmentPtr)
	return int64(rowCount)
}

func (s *Segment) getDeletedCount() int64 {
	/*
		long int
		getDeletedCount(CSegmentBase c_segment);
	*/
	var deletedCount = C.GetDeletedCount(s.segmentPtr)
	return int64(deletedCount)
}

func (s *Segment) getMemSize() int64 {
	/*
		long int
		GetMemoryUsageInBytes(CSegmentBase c_segment);
	*/
	var memoryUsageInBytes = C.GetMemoryUsageInBytes(s.segmentPtr)

	return int64(memoryUsageInBytes)
}

//-------------------------------------------------------------------------------------- preDm functions
func (s *Segment) segmentPreInsert(numOfRecords int) int64 {
	/*
		long int
		PreInsert(CSegmentBase c_segment, long int size);
	*/
	var offset = C.PreInsert(s.segmentPtr, C.long(int64(numOfRecords)))

	return int64(offset)
}

func (s *Segment) segmentPreDelete(numOfRecords int) int64 {
	/*
		long int
		PreDelete(CSegmentBase c_segment, long int size);
	*/
	var offset = C.PreDelete(s.segmentPtr, C.long(int64(numOfRecords)))

	return int64(offset)
}

//-------------------------------------------------------------------------------------- dm & search functions
func (s *Segment) segmentInsert(offset int64, entityIDs *[]UniqueID, timestamps *[]Timestamp, records *[]*commonpb.Blob) error {
	/*
		CStatus
		Insert(CSegmentBase c_segment,
		           long int reserved_offset,
		           signed long int size,
		           const long* primary_keys,
		           const unsigned long* timestamps,
		           void* raw_data,
		           int sizeof_per_row,
		           signed long int count);
	*/
	// Blobs to one big blob
	var numOfRow = len(*entityIDs)
	var sizeofPerRow = len((*records)[0].Value)

	assert.Equal(nil, numOfRow, len(*records))

	var rawData = make([]byte, numOfRow*sizeofPerRow)
	var copyOffset = 0
	for i := 0; i < len(*records); i++ {
		copy(rawData[copyOffset:], (*records)[i].Value)
		copyOffset += sizeofPerRow
	}

	var cOffset = C.long(offset)
	var cNumOfRows = C.long(numOfRow)
	var cEntityIdsPtr = (*C.long)(&(*entityIDs)[0])
	var cTimestampsPtr = (*C.ulong)(&(*timestamps)[0])
	var cSizeofPerRow = C.int(sizeofPerRow)
	var cRawDataVoidPtr = unsafe.Pointer(&rawData[0])

	var status = C.Insert(s.segmentPtr,
		cOffset,
		cNumOfRows,
		cEntityIdsPtr,
		cTimestampsPtr,
		cRawDataVoidPtr,
		cSizeofPerRow,
		cNumOfRows)

	errorCode := status.error_code

	if errorCode != 0 {
		errorMsg := C.GoString(status.error_msg)
		defer C.free(unsafe.Pointer(status.error_msg))
		return errors.New("Insert failed, C runtime error detected, error code = " + strconv.Itoa(int(errorCode)) + ", error msg = " + errorMsg)
	}

	s.recentlyModified = true
	return nil
}

func (s *Segment) segmentDelete(offset int64, entityIDs *[]UniqueID, timestamps *[]Timestamp) error {
	/*
		CStatus
		Delete(CSegmentBase c_segment,
		           long int reserved_offset,
		           long size,
		           const long* primary_keys,
		           const unsigned long* timestamps);
	*/
	var cOffset = C.long(offset)
	var cSize = C.long(len(*entityIDs))
	var cEntityIdsPtr = (*C.long)(&(*entityIDs)[0])
	var cTimestampsPtr = (*C.ulong)(&(*timestamps)[0])

	var status = C.Delete(s.segmentPtr, cOffset, cSize, cEntityIdsPtr, cTimestampsPtr)

	errorCode := status.error_code

	if errorCode != 0 {
		errorMsg := C.GoString(status.error_msg)
		defer C.free(unsafe.Pointer(status.error_msg))
		return errors.New("Delete failed, C runtime error detected, error code = " + strconv.Itoa(int(errorCode)) + ", error msg = " + errorMsg)
	}

	return nil
}

func (s *Segment) segmentSearch(plan *Plan,
	placeHolderGroups []*PlaceholderGroup,
	timestamp []Timestamp) (*SearchResult, error) {
	/*
		CStatus
		Search(void* plan,
			void* placeholder_groups,
			uint64_t* timestamps,
			int num_groups,
			long int* result_ids,
			float* result_distances);
	*/

	cPlaceholderGroups := make([]C.CPlaceholderGroup, 0)
	for _, pg := range placeHolderGroups {
		cPlaceholderGroups = append(cPlaceholderGroups, (*pg).cPlaceholderGroup)
	}

	var searchResult SearchResult
	var cTimestamp = (*C.ulong)(&timestamp[0])
	var cPlaceHolder = (*C.CPlaceholderGroup)(&cPlaceholderGroups[0])
	var cNumGroups = C.int(len(placeHolderGroups))
	var cQueryResult = (*C.CQueryResult)(&searchResult.cQueryResult)

	var status = C.Search(s.segmentPtr, plan.cPlan, cPlaceHolder, cTimestamp, cNumGroups, cQueryResult)
	errorCode := status.error_code

	if errorCode != 0 {
		errorMsg := C.GoString(status.error_msg)
		defer C.free(unsafe.Pointer(status.error_msg))
		return nil, errors.New("Search failed, C runtime error detected, error code = " + strconv.Itoa(int(errorCode)) + ", error msg = " + errorMsg)
	}

	return &searchResult, nil
}

func (s *Segment) fillTargetEntry(plan *Plan,
	result *SearchResult) error {

	var status = C.FillTargetEntry(s.segmentPtr, plan.cPlan, result.cQueryResult)
	errorCode := status.error_code

	if errorCode != 0 {
		errorMsg := C.GoString(status.error_msg)
		defer C.free(unsafe.Pointer(status.error_msg))
		return errors.New("FillTargetEntry failed, C runtime error detected, error code = " + strconv.Itoa(int(errorCode)) + ", error msg = " + errorMsg)
	}

	return nil
}

// segment, err := loadIndexService.replica.getSegmentByID(segmentID)
func (s *Segment) updateSegmentIndex(loadIndexInfo *LoadIndexInfo) error {
	status := C.UpdateSegmentIndex(s.segmentPtr, loadIndexInfo.cLoadIndexInfo)
	errorCode := status.error_code

	if errorCode != 0 {
		errorMsg := C.GoString(status.error_msg)
		defer C.free(unsafe.Pointer(status.error_msg))
		return errors.New("updateSegmentIndex failed, C runtime error detected, error code = " + strconv.Itoa(int(errorCode)) + ", error msg = " + errorMsg)
	}

	return nil
}