# A palindrome is a string that is exactly the same backwards and forward (e.g. anna).
# Whitespace is significant in this version, so "ahahaha" is a palindrome while "aha haha" is not.

.text
main:
	#la $4, string_space
	#li $5, 2048
	#li $2, 8 # read_string
	#syscall

        li $1, 5
        li $3, 0
        la $5, string1
	la $9,  string1	# A = S
	la $10, string1	# we need to move B to the end
# compute length of the string (strlen)
length_loop:
        addi $3, $3, 1
	lb   $11, ($10)                 # load the byte at B into $11.
	beqz $11, end_length_loop       # if $11 == 0, branch out of loop.
	addi $10, $10, 1                # otherwise, increment B,
	b length_loop                   # and loop

end_length_loop:
        subi $1, $1, 1
        add $5, $5, $3                  # move to next string
        subi $10, $10, 2                # subtract 2 to move B back past the '\0' and '\n'.

test_loop:
   bge $9, $10, is_palin           # if A >= B, it's a palindrome.
	lb  $11, ($9)                   # load the byte at addr A into $11,
	lb  $12, ($10)                  # load the byte at addr B into $12.
	bne $11, $12, not_palin         # if $11 != $12, not a palindrome.
	addi $9,  $9,  1                # increment A,
	subi $10, $10, 1                # decrement B,
	b test_loop                     # and loop

# print is_palin_msg, exit
is_palin:
	la $4, is_palin_msg
	li $2, 4
	syscall
	b exit

# print not_palin_msg, exit
not_palin:
   la $4, not_palin_msg
	li $2, 4
	syscall
	b exit

exit:
        beqz $1, exit2
        addi $9, $5, 0
        addi $10, $5, 0
        li $3, 0 
        b length_loop
exit2:        
   li $2, 10 # exit
	syscall

.data
is_palin_msg:     .asciiz "is a palindrome.\n"
not_palin_msg:    .asciiz "is not a palindrome.\n"
#string_space:     .space 2048 # reserve space for computation
string1:     .asciiz "abttttttttttttttttttttttttttttba\n"
string2:     .asciiz "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n"
string3:     .asciiz "abaaaaaaaaaaaaaaaaaaaaaaaaaaaba\n"
string4:     .asciiz "sdgkadsjglksalkgslalskdlfkkdkdklekwlernwelkrnwelkrnlwekrlnkrwnelkrklwelkewrlkweklewew\n"
string5:     .asciiz "dkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkdkd\n"

