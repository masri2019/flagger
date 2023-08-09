from block_utils import *
from falsifier_utils import *
import unittest
from project_blocks_multi_thread import makeCigarString

class TestProjection(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        """Define two alignments one in positive orientation and one in negative"""
        self.alignment1 = Alignment("ctg2\t30\t8\t18\t+\tctg1\t30\t6\t15\t8\t9\t60\tcg:Z:1=1X2I3=1D3=\ttp:A:P")
        self.alignment2 = Alignment("ctg2\t30\t21\t27\t-\tctg1\t30\t20\t27\t5\t7\tcg:Z:3=2D1X1I1=\ttp:A:P")
        self.alignments = [self.alignment1, self.alignment2]
        self.contigLengths = {"ctg1":30, "ctg2":30, "ctg3":10}
        print(f"Tests:")

    def testHomologyRelationsDict(self):
        outputRelations = HomologyRelation.createAllInclusiveRelationDictFromAlignments(self.alignments, self.contigLengths, "_f")

        ctg1HomologyBlock1 = HomologyBlock("ctg1", 1, 6, '+', "ctg1_f", 0)
        ctg1HomologyBlock2 = HomologyBlock("ctg1", 7, 15, '+', "ctg1_f", 1)
        ctg1HomologyBlock3 = HomologyBlock("ctg1", 16, 20, '+', "ctg1_f", 2)
        ctg1HomologyBlock4 = HomologyBlock("ctg1", 21, 27, '+', "ctg1_f", 3)
        ctg1HomologyBlock5 = HomologyBlock("ctg1", 28, 30, '+', "ctg1_f", 4)

        ctg2HomologyBlock1 = HomologyBlock("ctg2", 1, 8, '+', "ctg2_f", 0)
        ctg2HomologyBlock2 = HomologyBlock("ctg2", 9, 18, '+', "ctg2_f", 1)
        ctg2HomologyBlock3 = HomologyBlock("ctg2", 19, 21, '+', "ctg2_f", 2)
        ctg2HomologyBlock4 = HomologyBlock("ctg2", 22, 27, '+', "ctg2_f", 3)
        ctg2HomologyBlock5 = HomologyBlock("ctg2", 28, 30, '+', "ctg2_f", 4)

        ctg3HomologyBlock1 = HomologyBlock("ctg3", 1, 10, '+', "ctg3_f", 0)

        #blockAlignment1 = Alignment(f"query_ctg\t10\t0\t10\t+\tref_ctg\t9\t0\t9\t0\t0\t60\tcg:Z:1=1X2I3=1D3=\ttp:A:P")
        #blockAlignment2 = Alignment(f"query_ctg\t11\t0\t11\t+\tref_ctg\t10\t0\t10\t0\t0\t60\tcg:Z:3=2D1X1I1=\ttp:A:P")

        truthRelations = {"ctg1" : [HomologyRelation(ctg1HomologyBlock1, None, None, None),
                               HomologyRelation(ctg1HomologyBlock2, ctg2HomologyBlock2, getCigarList("1=1X2I3=1D3="), '+'),
                               HomologyRelation(ctg1HomologyBlock3, None, None, None),
                               HomologyRelation(ctg1HomologyBlock4, ctg2HomologyBlock4, getCigarList("3=2D1X1I1="), '-'),
                               HomologyRelation(ctg1HomologyBlock5, None, None, None)],
                     "ctg2": [HomologyRelation(ctg2HomologyBlock1, None, None, None),
                              HomologyRelation(ctg2HomologyBlock2, ctg1HomologyBlock2, getCigarList("1=1X2I3=1D3="), '+'),
                              HomologyRelation(ctg2HomologyBlock3, None, None, None),
                              HomologyRelation(ctg2HomologyBlock4, ctg1HomologyBlock4, getCigarList("3=2D1X1I1="), '-'),
                              HomologyRelation(ctg2HomologyBlock5, None, None, None)],
                     "ctg3": [HomologyRelation(ctg3HomologyBlock1, None, None, None)]}

        self.assertDictEqual(truthRelations, outputRelations)


def main():
    unittest.main(verbosity=2)

if __name__ == '__main__':
    main()

