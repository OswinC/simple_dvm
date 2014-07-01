public class GlobalVariables extends DhrystoneConstants {

    static Record_Type          Record_Glob,
                                Next_Record_Glob;
    static int                  Int_Glob;
    static boolean              Bool_Glob;
    static char                 Char_Glob_1,
                                Char_Glob_2;
    static int[]                Array_Glob_1    = new int[128];
    static int[][]              Array_Glob_2    = new int[128][128];
    static Record_Type          First_Record    = new Record_Type(),
                                Second_Record   = new Record_Type();
}
