from block_utils import findProjections, Alignment
import unittest
from project_blocks_multi_thread import makeCigarString

class TestProjection(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        """Define two alignments one in positive orientation and one in negative"""
        self.alignmentPositive = Alignment("ctg\t350\t100\t150\t+\tref\t158\t10\t58\t27\t48\t60\tcg:Z:4=1X2I2=1X10D1X2=10I1X1=5D5=1X4=5I3=1X6=\ttp:A:P")
        self.alignmentNegative = Alignment("ctg\t350\t200\t250\t-\tref\t158\t10\t58\t27\t48\t60\tcg:Z:4=1X2I2=1X10D1X2=10I1X1=5D5=1X4=5I3=1X6=\ttp:A:P")


    def testPositiveAsm2Ref(self):
        alignment = self.alignmentPositive
        mode = "asm2ref"
        includeEndingIndel = False
        includePostIndel = False
        # block start and end are 1-based
        blocks = [(81, 106, "NA"), (111, 116, "NA"), (124, 126, "NA")]
        projectionBlocks = [(11, 15), (29, 31), (32, 39)]
        projectableBlocks = [(101, 105), (111, 113), (124, 126)]
        cigarListTruth = ["4=1X", "1X2=", "1X1=5D1="]
        qBlocks, rBlocks, cigarList = findProjections(mode,
                                                      alignment.cigarList,
                                                      blocks,
                                                      alignment.chromLength,
                                                      alignment.chromStart + 1, alignment.chromEnd, # make 1-based start
                                                      alignment.contigLength,
                                                      alignment.contigStart + 1, alignment.contigEnd, # make 1-based start
                                                      alignment.orientation,
                                                      includeEndingIndel, includePostIndel)
        for i in range(3):
            self.assertEqual(qBlocks[i][0], projectableBlocks[i][0], "Incorrect projectable start position")
            self.assertEqual(qBlocks[i][1], projectableBlocks[i][1], "Incorrect projectable end position")
            self.assertEqual(rBlocks[i][0], projectionBlocks[i][0], "Incorrect projection start position")
            self.assertEqual(rBlocks[i][1], projectionBlocks[i][1], "Incorrect projection end position")
            self.assertEqual(cigarListTruth[i], makeCigarString(cigarList[i]), "Incorrect CIGAR string")

    def testPositiveAsm2RefIncludeEndingIndel(self):
        alignment = self.alignmentPositive
        mode = "asm2ref"
        includeEndingIndel = True
        includePostIndel = False
        # block start and end are 1-based
        blocks = [(101, 110, "NA"), (111, 116, "NA"), (117, 126, "NA")]
        projectionBlocks = [(11, 18), (29, 31), (32, 39)]
        projectableBlocks = [(101, 110), (111, 116), (117, 126)]
        cigarListTruth = ["4=1X2I2=1X", "1X2=3I", "7I1X1=5D1="]
        qBlocks, rBlocks, cigarList = findProjections(mode,
                                                      alignment.cigarList,
                                                      blocks,
                                                      alignment.chromLength,
                                                      alignment.chromStart + 1, alignment.chromEnd, # make 1-based start
                                                      alignment.contigLength,
                                                      alignment.contigStart + 1, alignment.contigEnd, # make 1-based start
                                                      alignment.orientation,
                                                      includeEndingIndel, includePostIndel)
        for i in range(3):
            self.assertEqual(qBlocks[i][0], projectableBlocks[i][0], "Incorrect projectable start position")
            self.assertEqual(qBlocks[i][1], projectableBlocks[i][1], "Incorrect projectable end position")
            self.assertEqual(rBlocks[i][0], projectionBlocks[i][0], "Incorrect projection start position")
            self.assertEqual(rBlocks[i][1], projectionBlocks[i][1], "Incorrect projection end position")
            self.assertEqual(cigarListTruth[i], makeCigarString(cigarList[i]), "Incorrect CIGAR string")

    def testPositiveAsm2RefIncludeEndingAndPostIndel(self):
        alignment = self.alignmentPositive
        mode = "asm2ref"
        includeEndingIndel = True
        includePostIndel = True
        # block start and end are 1-based
        blocks = [(101, 110, "NA"), (111, 116, "NA"), (117, 126, "NA")]
        projectionBlocks = [(11, 28), (29, 31), (32, 39)]
        projectableBlocks = [(101, 110), (111, 116), (117, 126)]
        cigarListTruth = ["4=1X2I2=1X10D", "1X2=3I", "7I1X1=5D1="]
        qBlocks, rBlocks, cigarList = findProjections(mode,
                                                      alignment.cigarList,
                                                      blocks,
                                                      alignment.chromLength,
                                                      alignment.chromStart + 1, alignment.chromEnd, # make 1-based start
                                                      alignment.contigLength,
                                                      alignment.contigStart + 1, alignment.contigEnd, # make 1-based start
                                                      alignment.orientation,
                                                      includeEndingIndel, includePostIndel)
        for i in range(3):
            self.assertEqual(qBlocks[i][0], projectableBlocks[i][0], "Incorrect projectable start position")
            self.assertEqual(qBlocks[i][1], projectableBlocks[i][1], "Incorrect projectable end position")
            self.assertEqual(rBlocks[i][0], projectionBlocks[i][0], "Incorrect projection start position")
            self.assertEqual(rBlocks[i][1], projectionBlocks[i][1], "Incorrect projection end position")
            self.assertEqual(cigarListTruth[i], makeCigarString(cigarList[i]), "Incorrect CIGAR string")

    def testPositiveRef2Asm(self):
        alignment = self.alignmentPositive
        mode = "ref2asm"
        includeEndingIndel = False
        includePostIndel = False
        # block start and end are 1-based
        blocks = [(11, 16, "NA"), (29, 31, "NA"), (32, 35, "NA")]
        projectionBlocks = [(101, 108), (111, 113), (124,125)]
        projectableBlocks = [(11, 16), (29, 31), (32, 33)]
        cigarListTruth = ["4=1X2I1=", "1X2=", "1X1="]
        qBlocks, rBlocks, cigarList = findProjections(mode,
                                                      alignment.cigarList,
                                                      blocks,
                                                      alignment.chromLength,
                                                      alignment.chromStart + 1, alignment.chromEnd, # make 1-based start
                                                      alignment.contigLength,
                                                      alignment.contigStart + 1, alignment.contigEnd, # make 1-based start
                                                      alignment.orientation,
                                                      includeEndingIndel, includePostIndel)
        for i in range(3):
            self.assertEqual(qBlocks[i][0], projectableBlocks[i][0], "Incorrect projectable start position")
            self.assertEqual(qBlocks[i][1], projectableBlocks[i][1], "Incorrect projectable end position")
            self.assertEqual(rBlocks[i][0], projectionBlocks[i][0], "Incorrect projection start position")
            self.assertEqual(rBlocks[i][1], projectionBlocks[i][1], "Incorrect projection end position")
            self.assertEqual(cigarListTruth[i], makeCigarString(cigarList[i]), "Incorrect CIGAR string")

    def testPositiveRef2AsmInclude(self):
        alignment = self.alignmentPositive
        mode = "ref2asm"
        includeEndingIndel = False
        includePostIndel = False
        # block start and end are 1-based
        blocks = [(11, 16, "NA"), (29, 31, "NA"), (32, 35, "NA")]
        projectionBlocks = [(101, 108), (111, 113), (124,125)]
        projectableBlocks = [(11, 16), (29, 31), (32, 33)]
        cigarListTruth = ["4=1X2I1=", "1X2=", "1X1="]
        qBlocks, rBlocks, cigarList = findProjections(mode,
                                                      alignment.cigarList,
                                                      blocks,
                                                      alignment.chromLength,
                                                      alignment.chromStart + 1, alignment.chromEnd, # make 1-based start
                                                      alignment.contigLength,
                                                      alignment.contigStart + 1, alignment.contigEnd, # make 1-based start
                                                      alignment.orientation,
                                                      includeEndingIndel, includePostIndel)
        for i in range(3):
            self.assertEqual(qBlocks[i][0], projectableBlocks[i][0], "Incorrect projectable start position")
            self.assertEqual(qBlocks[i][1], projectableBlocks[i][1], "Incorrect projectable end position")
            self.assertEqual(rBlocks[i][0], projectionBlocks[i][0], "Incorrect projection start position")
            self.assertEqual(rBlocks[i][1], projectionBlocks[i][1], "Incorrect projection end position")
            self.assertEqual(cigarListTruth[i], makeCigarString(cigarList[i]), "Incorrect CIGAR string")

unittest.main()
